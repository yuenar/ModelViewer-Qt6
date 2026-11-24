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
  // QRhiWidget::initialize 的同款两分支：首次创建 / 设备或目标变化重建
  if (!m_renderer) {
    if (cb) {
      initializeRenderer(cb);
    } else {
      // cb 缺失（窗口刚暴露等场景）：推迟到 render() 里完成
      m_pendingInit = true;
    }
  } else {
    m_renderer->setRenderTarget(renderTarget());
    m_renderer->rebuildPipelines();
  }
}

void MobileRhiItemRenderer::initializeRenderer(QRhiCommandBuffer* cb) {
  if (m_renderer || !rhi() || !renderTarget()) {
    return;
  }
  m_renderer = new RhiRenderer(rhi(), renderTarget());
  m_renderer->initialize(cb);
  m_pendingInit = false;
}

void MobileRhiItemRenderer::synchronize(QQuickRhiItem* item) {
  auto* rhiItem = static_cast<MobileRhiItem*>(item);
  m_bridge = rhiItem->bridge();
  if (!m_bridge || !m_renderer) {
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
  if (m_pendingInit && cb) {
    initializeRenderer(cb);
  }
  if (!m_renderer || !cb) {
    return;
  }
  if (m_renderPaused) {
    return;  // 后台暂停：跳过 GPU 工作（QQuickRhiItem 按需渲染，无空转）
  }

  const QSize pixelSize = renderTarget()->pixelSize();
  if (pixelSize.height() > 0) {
    m_camera.setAspectRatio(float(pixelSize.width()) /
                            float(pixelSize.height()));
  }

  // 与桌面 RhiWidget::render 完全一致的核心调用（内部自动上传网格）
  m_renderer->render(cb, m_meshes, m_camera, m_light, m_material);

  // 截图：绘制完成后从离屏颜色纹理读回（GLES 后端同步完成）
  if (m_grabRequested) {
    grabFrame(cb);
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

void MobileRhiItemRenderer::grabFrame(QRhiCommandBuffer* cb) {
  QRhiTexture* color = colorTexture();
  if (!color || !rhi()) {
    return;
  }
  const QSize size = color->pixelSize();
  if (size.isEmpty()) {
    return;
  }

  QRhiResourceUpdateBatch* batch = rhi()->nextResourceUpdateBatch();
  QRhiReadbackDescription desc;
  desc.setTexture(color);
  QRhiReadbackResult result;
  batch->readBackTexture(desc, &result);
  cb->resourceUpdate(batch);

  // OpenGL ES 后端 readback 同步完成；若后续切到 Vulkan/Metal 后端，
  // 需改用 QRhiReadbackResult::completed 回调交付
  if (result.data.size() >= size.width() * size.height() * 4) {
    m_grabbedImage = QImage(
        reinterpret_cast<const uchar*>(result.data.constData()),
        size.width(), size.height(), QImage::Format_RGBA8888).copy();
    // 若真机上出现上下颠倒，在此处 .mirrored() 修正一次即可
  }
  // 图片在下一次 synchronize() 交给 Bridge（GUI 线程）保存
}

void MobileRhiItemRenderer::flushStats() {
  if (!m_bridge || !m_renderer) {
    return;
  }
  const RenderStats& s = m_renderer->stats();
  m_bridge->setStatsSnapshot(s.objects, s.drawn, s.culled, s.drawCalls,
                             s.triangles, s.lod0, s.lod1, s.lod2, m_fps);
}
