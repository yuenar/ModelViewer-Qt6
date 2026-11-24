// Copyright 2026 ModelViewer Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "MobileRhiItem.h"

#include <QDebug>
#include <QVector>
#include <rhi/qrhi.h>

#include "MobileViewerBridge.h"
#include "../renderer/RhiMesh.h"
#include "../renderer/RhiRenderer.h"

// ── MobileRhiItem（GUI 线程）───────────────────────────────────────

MobileRhiItem::MobileRhiItem(QQuickItem* parent)
    : QQuickRhiItem(parent) {
  // 移动端默认性能策略：MSAA 关闭（可由 QML 设 sampleCount 开 2/4）
  setSampleCount(1);
}

MobileRhiItem::~MobileRhiItem() = default;

void MobileRhiItem::setBridge(MobileViewerBridge* bridge) {
  if (m_bridge == bridge) {
    return;
  }
  if (m_bridge) {
    disconnect(m_bridge, nullptr, this, nullptr);
  }
  m_bridge = bridge;
  emit bridgeChanged();
  if (m_bridge) {
    // Bridge 的重绘请求 → 调度 updatePaintNode → synchronize + render
    connect(m_bridge, &MobileViewerBridge::frameUpdateRequested, this,
            [this]() { update(); });
    update();
  }
}

QQuickRhiItemRenderer* MobileRhiItem::createRenderer() {
  return new MobileRhiItemRenderer();
}

// ── MobileRhiItemRenderer（渲染线程）───────────────────────────────

MobileRhiItemRenderer::~MobileRhiItemRenderer() {
  delete m_readback;
  m_readback = nullptr;
  // 与桌面 ~RhiWidget 相同的释放序列（渲染线程执行）：
  // 先把网格 GPU 缓冲归还（不删除 CPU 数据），再销毁渲染器
  if (m_renderer) {
    for (RhiMesh* mesh : m_meshes) {
      if (mesh) {
        mesh->setBufferPool(nullptr);
        mesh->releaseGpu();
      }
    }
    delete m_renderer;
    m_renderer = nullptr;
  }
}

void MobileRhiItemRenderer::initialize(QRhiCommandBuffer* cb) {
  // 注意：Qt 每次 sync 都会调用本函数（并非只在首次/尺寸变化时），因此
  // 不能无条件重建管线——Vulkan 下销毁仍在飞行帧中使用的管线是未定义行为。
  // 只有渲染目标对象真正换了（格式/重建）才重建设备资源。
  QRhiRenderTarget* rt = renderTarget();
  if (!m_renderer) {
    // 目标尚未就绪时推迟到 render() 里补建，避免永久黑屏
    if (cb && rt) {
      initializeRenderer(cb);
    } else {
      m_pendingInit = true;
    }
    return;
  }
  if (rt && rt != m_lastRt) {
    m_renderer->setRenderTarget(rt);
    m_renderer->rebuildPipelines();
  }
  m_lastRt = rt;
}

void MobileRhiItemRenderer::initializeRenderer(QRhiCommandBuffer* cb) {
  if (m_renderer || !rhi() || !renderTarget()) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      qWarning() << "[RhiItem] initializeRenderer skipped: rhi"
                 << (rhi() != nullptr) << "rt" << (renderTarget() != nullptr);
    }
    return;
  }
  m_renderer = new RhiRenderer(rhi(), renderTarget());
  m_lastRt = renderTarget();
  m_renderer->initialize(cb);
  m_pendingInit = false;
}

void MobileRhiItemRenderer::synchronize(QQuickRhiItem* item) {
  auto* rhiItem = static_cast<MobileRhiItem*>(item);
  m_item = rhiItem;
  m_bridge = rhiItem->bridge();
  if (!m_bridge) {
    return;
  }

  // ① 退役网格：在渲染线程释放 GPU 资源并删除（GUI 线程此刻阻塞）
  m_bridge->reclaimRetiredMeshes();

  // ② 相机快照（Camera 禁拷贝，逐字段同步；fov 两侧恒为默认 45°）
  const Camera& cam = m_bridge->camera();
  m_camera.setPosition(cam.position());
  m_camera.setTarget(cam.target());
  m_camera.setUp(cam.upDirection());
  m_camera.setProjectionType(cam.projectionType());
  // aspect 在 render() 中按实际像素尺寸设置

  // ③ 场景数据与渲染参数（值拷贝 / 指针快照）
  m_light = m_bridge->light();
  m_material = m_bridge->material();
  m_meshes = m_bridge->meshes();  // 旧对象在 retired 队列里保活，指针稳定
  m_renderPaused = m_bridge->isRenderPaused();

  // 渲染器可能尚未创建（Vulkan 下 initialize() 拿到 cb 的时机不同）：
  // 场景快照照常更新，render() 补建渲染器后本帧即可直接使用
  if (!m_renderer) {
    return;
  }

  m_renderer->setRenderMode(m_bridge->renderMode());
  m_renderer->setDisplayStyle(m_bridge->displayStyle());
  m_renderer->setMultiView(m_bridge->multiView());
  m_renderer->setAxisVisible(m_bridge->axisVisible());
  m_renderer->setGridVisible(m_bridge->gridVisible());
  m_renderer->setCullingEnabled(m_bridge->cullingEnabled());
  m_renderer->setLodEnabled(m_bridge->lodEnabled());
  m_renderer->setNormalLength(m_bridge->normalLength());
  m_renderer->setClipPlane(m_bridge->clipPlane());
  m_renderer->setSelectedMesh(m_bridge->selectedMesh());
  m_renderer->setBackgroundColors(m_bridge->backgroundTopColor(),
                                  m_bridge->backgroundBottomColor());

  const QImage diffuse = m_bridge->takePendingDiffuseTexture();
  if (!diffuse.isNull()) {
    m_renderer->setDiffuseTexture(diffuse);
  }

  // ④ 变更网格数据（动画 / 新模型）后重置上传标志
  if (m_bridge->takeUploadResetRequest()) {
    m_renderer->resetMeshUploadFlag();
  }

  // ⑤ 截图请求 / 截图交付
  if (m_bridge->takeScreenshotRequest()) {
    m_grabRequested = true;  // 本帧渲染完成后 readback
  }
  if (!m_grabbedImage.isNull()) {
    m_bridge->deliverScreenshot(m_grabbedImage);
    m_grabbedImage = QImage();
  }
}

void MobileRhiItemRenderer::render(QRhiCommandBuffer* cb) {
  // 异步后端（Vulkan）上一帧 readback 已就绪：在此组装图像，
  // 下一次 synchronize() 交付给 Bridge（GUI 线程）
  if (m_readback && m_readbackReady) {
    const int bytes = m_readbackSize.width() * m_readbackSize.height() * 4;
    if (m_readbackSize.isValid() && m_readback->data.size() >= bytes) {
      m_grabbedImage =
          QImage(reinterpret_cast<const uchar*>(m_readback->data.constData()),
                 m_readbackSize.width(), m_readbackSize.height(),
                 QImage::Format_RGBA8888)
              .copy();
    }
    delete m_readback;
    m_readback = nullptr;
    m_readbackReady = false;
  }

  if (m_pendingInit || !m_renderer) {
    if (cb) {
      initializeRenderer(cb);
    }
  }
  if (!m_renderer || !cb) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      qWarning() << "[RhiItem] render skipped: renderer" << (m_renderer != nullptr)
                 << "cb" << (cb != nullptr);
    }
    return;
  }
  if (m_renderPaused) {
    return;  // 后台暂停：跳过 GPU 工作（QQuickRhiItem 按需渲染，无空转）
  }

  // 渲染目标对象变化（格式/采样数变化时 Qt 会重建 RT）时同步给渲染器
  QRhiRenderTarget* rt = renderTarget();
  if (rt && rt != m_lastRt) {
    m_renderer->setRenderTarget(rt);
    m_renderer->rebuildPipelines();
    m_lastRt = rt;
  }

  const QSize pixelSize = renderTarget()->pixelSize();
  if (pixelSize.height() > 0) {
    m_camera.setAspectRatio(float(pixelSize.width()) /
                            float(pixelSize.height()));
  }

  // 与桌面 RhiWidget::render 完全一致的核心调用（内部自动上传网格）
  m_renderer->render(cb, m_meshes, m_camera, m_light, m_material);

  // 截图：绘制完成后从离屏颜色纹理读回（GLES 同步 / Vulkan 异步）
  if (m_grabRequested && grabFrame(cb)) {
    m_grabRequested = false;
  }

  // FPS 统计（1 秒窗口；统计文本经 queued 信号回 GUI 线程）
  ++m_frameCounter;
  if (m_fpsTimer.isValid() && m_fpsTimer.elapsed() >= 1000) {
    m_fps = float(m_frameCounter) * 1000.0f / float(m_fpsTimer.elapsed());
    m_frameCounter = 0;
    m_fpsTimer.restart();
    flushStats();
  } else if (!m_fpsTimer.isValid()) {
    m_fpsTimer.start();
  }
}

bool MobileRhiItemRenderer::grabFrame(QRhiCommandBuffer* cb) {
  QRhiTexture* color = colorTexture();
  if (!color || !rhi()) {
    return false;
  }
  const QSize size = color->pixelSize();
  if (size.isEmpty()) {
    return false;
  }

  QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
  QRhiReadbackDescription desc;
  desc.setTexture(color);

  if (rhi()->backend() == QRhi::OpenGLES2) {
    // GL 后端：readback 在 resourceUpdate 时同步完成，直接取图
    QRhiReadbackResult result;
    batch->readBackTexture(desc, &result);
    cb->resourceUpdate(batch);
    if (result.data.size() >= size.width() * size.height() * 4) {
      m_grabbedImage = QImage(
          reinterpret_cast<const uchar*>(result.data.constData()),
          size.width(), size.height(), QImage::Format_RGBA8888).copy();
    }
    // 图片在下一次 synchronize() 交给 Bridge（GUI 线程）保存
    return true;
  }

  // Vulkan 等异步后端：数据在帧提交后到达，completed 回调只置位，
  // 图像在下一帧 render() 开头组装（回调执行期间不能析构结果对象）
  if (m_readback) {
    return false;  // 上一次 readback 仍在途，保留请求下一帧重试
  }
  m_readback = new QRhiReadbackResult;
  m_readbackSize = size;
  m_readbackReady = false;
  m_readback->completed = [this]() {
    m_readbackReady = true;
    // 按需渲染：结果在帧提交后到达，必须再请求一帧才能组装并交付图像
    if (m_item) {
      MobileRhiItem* it = m_item;
      QMetaObject::invokeMethod(
          it, [it]() { it->update(); }, Qt::QueuedConnection);
    }
  };
  batch->readBackTexture(desc, m_readback);
  cb->resourceUpdate(batch);
  return true;
}

void MobileRhiItemRenderer::flushStats() {
  if (!m_bridge || !m_renderer) {
    return;
  }
  const RenderStats& s = m_renderer->stats();
  m_bridge->setStatsSnapshot(s.objects, s.drawn, s.culled, s.drawCalls,
                             s.triangles, s.lod0, s.lod1, s.lod2, m_fps);
}
