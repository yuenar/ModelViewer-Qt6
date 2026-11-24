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

#include "MobileViewerBridge.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>
#include <limits>

#include "../core/ParametricSurface.h"
#include "../loader/ModelLoader.h"
#include "../renderer/RhiMesh.h"

namespace {

/// @brief 统计三角形总数（供 modelLoaded 信号）
int totalTriangles(const QVector<CpuMesh>& meshes) {
  int tris = 0;
  for (const CpuMesh& mesh : meshes) {
    tris += mesh.indices.size() / 3;
  }
  return tris;
}

QString resolveScreenshotPath() {
  const QString stamp =
      QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  const QString fileName = QStringLiteral("modelviewer_%1.png").arg(stamp);
  // Android 10+ 分区存储下直接写公共 Pictures 可能失败，回退到应用
  // 私有目录（无需权限）
  const QStringList candidates = {
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
          QStringLiteral("/Pictures")};
  for (const QString& dir : candidates) {
    if (dir.isEmpty() || !QDir().mkpath(dir)) {
      continue;
    }
    const QString path = dir + QLatin1Char('/') + fileName;
    QFileInfo info(path);
    if (info.isWritable() || !info.exists()) {
      return path;
    }
  }
  return QString();
}

}  // namespace

MobileViewerBridge::MobileViewerBridge(QObject* parent) : QObject(parent) {
  // ── 动画推进定时器（仅播放时运行；空闲时场景不重绘 = 省电）────
  m_animTimer = new QTimer(this);
  m_animTimer->setInterval(16);  // ~60fps；低端机可调 33（30fps）
  connect(m_animTimer, &QTimer::timeout, this,
          &MobileViewerBridge::advanceAnimation);

  // ── 后台暂停：应用失活时停渲染（对应扩展文档 §7.4，实现方式为
  //    暂停动画定时器 + 渲染线程 render() 早退）
  connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
          &MobileViewerBridge::onApplicationStateChanged);
}

MobileViewerBridge::~MobileViewerBridge() {
  stopAnimationInternal();
  // 与桌面 ~RhiWidget 相同的释放序列（进程退出路径）
  for (RhiMesh* mesh : m_meshes) {
    if (mesh) {
      mesh->setBufferPool(nullptr);
      mesh->releaseGpu();
    }
  }
  qDeleteAll(m_meshes);
  qDeleteAll(m_retired);
  m_meshes.clear();
  m_retired.clear();
}

// ── 模型加载 ───────────────────────────────────────────────────────

void MobileViewerBridge::loadModel(const QString& path) {
  if (m_loading || path.isEmpty()) {
    return;
  }
  if (!QFile::exists(path)) {
    emit loadError(QStringLiteral("文件不存在: %1").arg(path));
    return;
  }

  m_loading = true;
  emit loadingChanged();
  emit loadStarted();
  emit loadProgress(0.05);

  // Android 上大模型加载在后台线程，避免阻塞 UI（BusyIndicator）
  auto* watcher = new QFutureWatcher<ModelAsset>(this);
  connect(watcher, &QFutureWatcher<ModelAsset>::finished, this,
          [this, watcher, path]() {
            watcher->deleteLater();
            const ModelAsset asset = watcher->result();

            m_loading = false;
            emit loadingChanged();

            if (asset.meshes.isEmpty()) {
              emit loadError(
                  QStringLiteral("无法加载模型: %1").arg(path));
              return;
            }
            emit loadProgress(0.9);
            setMeshesInternal(asset.meshes, path, asset.animations);
            emit loadProgress(1.0);
          });
  watcher->setFuture(QtConcurrent::run(
      [](const QString& file) {
        ModelLoader loader;
        return loader.loadCPU(file);
      },
      path));
}

void MobileViewerBridge::createSurface(int type) {
  CpuMesh mesh;
  switch (type) {
    case 0: mesh = ParametricSurface::plane();  break;
    case 1: mesh = ParametricSurface::sphere(); break;
    case 2: mesh = ParametricSurface::torus();  break;
    case 3: mesh = ParametricSurface::cylinder(); break;
    case 4: mesh = ParametricSurface::cone();   break;
    default: return;
  }
  QVector<CpuMesh> meshes;
  meshes.append(mesh);
  static const QStringList kSurfaceNames = {
      QStringLiteral("平面"), QStringLiteral("球体"),
      QStringLiteral("圆环"), QStringLiteral("圆柱"),
      QStringLiteral("圆锥")};
  const QString label =
      QStringLiteral("曲面/") + kSurfaceNames.value(type);
  emit loadStarted();
  setMeshesInternal(meshes, label, {});
}

void MobileViewerBridge::setMeshesInternal(QVector<CpuMesh> meshes,
                                            const QString& label,
                                            QVector<AnimationClip> animations) {
  // 旧网格交给渲染线程在下一次 synchronize 释放 GPU 资源后删除
  m_retired += m_meshes;
  m_meshes.clear();

  stopAnimationInternal();
  m_animationClips = animations;
  resetAnimationDeltas();
  emit animationChanged();

  m_sceneBbox = BoundingBox();
  m_selectedMesh = -1;

  int index = 0;
  for (const CpuMesh& cpuMesh : meshes) {
    auto* mesh = new RhiMesh(nullptr, cpuMesh);  // 仅 CPU 数据，线程安全
    mesh->setName(cpuMesh.name.isEmpty()
                      ? QString("Mesh %1").arg(index)
                      : cpuMesh.name);
    ++index;
    m_meshes.append(mesh);
    m_sceneBbox.expand(cpuMesh.bbox);
    if (!cpuMesh.diffuseTexture.isNull() && m_pendingDiffuse.isNull()) {
      m_pendingDiffuse = cpuMesh.diffuseTexture;
    }
  }

  QFileInfo info(label);
  m_modelLabel = info.isFile() ? info.fileName() : label;
  emit modelLabelChanged();

  m_camera.fitToView(m_sceneBbox);
  m_normalLength = qMax(m_sceneBbox.radius() * 0.15f, 0.05f);
  m_clipPlane = QVector4D(-1.0f, 0.0f, 0.0f, 0.0f);

  m_uploadResetRequested = true;
  emit meshesChanged();
  emit modelLoaded(m_modelLabel, m_meshes.size(),
                   totalTriangles(meshes));
  requestFrame();
}

// ── 渲染参数 ───────────────────────────────────────────────────────

void MobileViewerBridge::setRenderMode(int mode) {
  if (mode < 0 || mode > 11 || mode == m_renderMode) {
    return;
  }
  m_renderMode = mode;
  emit renderModeChanged(mode);
  requestFrame();
}

void MobileViewerBridge::setDisplayStyle(int style) {
  if (style < 0 || style > 3) {
    return;
  }
  m_displayStyle = style;
  requestFrame();
}

void MobileViewerBridge::setMultiView(bool enabled) {
  m_multiView = enabled;
  requestFrame();
}

void MobileViewerBridge::setOrthographic(bool ortho) {
  m_camera.setProjectionType(ortho ? ProjectionType::Orthographic
                                   : ProjectionType::Perspective);
  requestFrame();
}

// ── 视图控制 ───────────────────────────────────────────────────────

void MobileViewerBridge::resetView() {
  m_camera.reset();
  m_camera.fitToView(m_sceneBbox);
  requestFrame();
}

void MobileViewerBridge::fitView() {
  if (!m_sceneBbox.isEmpty()) {
    m_camera.fitToView(m_sceneBbox);
    requestFrame();
  }
}

void MobileViewerBridge::applyViewPreset(int preset) {
  if (preset < 0 || preset > 8) {
    return;
  }
  m_camera.setViewPreset(preset, m_sceneBbox);
  requestFrame();
}

// ── 手势（语义对齐桌面 RhiWidget 的鼠标处理）────────────────────────

void MobileViewerBridge::beginRotateGesture(qreal x, qreal y) {
  m_lastRotatePoint = QPoint(int(x), int(y));
  m_camera.beginRotate(m_lastRotatePoint);
  m_rotating = true;
}

void MobileViewerBridge::rotateAt(qreal x, qreal y, qreal viewportW,
                                  qreal viewportH) {
  if (!m_rotating || viewportW <= 0 || viewportH <= 0) {
    return;
  }
  // 注意：必须用花括号初始化，圆括号会被解析成函数声明
  const QPoint current{int(x), int(y)};
  m_camera.rotate(m_lastRotatePoint, current,
                  QSize(int(viewportW), int(viewportH)));
  m_lastRotatePoint = current;
  requestFrame();
}

void MobileViewerBridge::panGesture(qreal dx, qreal dy) {
  m_camera.pan(QPoint(int(dx), int(dy)));
  requestFrame();
}

void MobileViewerBridge::zoomGesture(qreal scaleDelta) {
  m_camera.zoom(float(scaleDelta));
  requestFrame();
}

// ── 拾取（逻辑移植自 RhiWidget::pickMesh / unproject）──────────────

QVector3D MobileViewerBridge::unproject(const QPoint& pos, float ndcZ,
                                        qreal viewportW,
                                        qreal viewportH) const {
  if (viewportW <= 0 || viewportH <= 0) {
    return QVector3D();
  }
  const float ndcX = 2.0f * float(pos.x()) / float(viewportW) - 1.0f;
  const float ndcY = 1.0f - 2.0f * float(pos.y()) / float(viewportH);

  const QMatrix4x4 proj = m_camera.projectionMatrix();
  const QMatrix4x4 view = m_camera.viewMatrix();
  const QMatrix4x4 inv = (proj * view).inverted();

  QVector4D p = inv * QVector4D(ndcX, ndcY, ndcZ, 1.0f);
  if (qAbs(p.w()) < 1e-6f) {
    return QVector3D();
  }
  return p.toVector3D() / p.w();
}

int MobileViewerBridge::pickMesh(const QPoint& pos, qreal viewportW,
                                 qreal viewportH) const {
  if (m_meshes.isEmpty()) {
    return -1;
  }

  const QVector3D nearPoint = unproject(pos, -1.0f, viewportW, viewportH);
  const QVector3D farPoint = unproject(pos, 1.0f, viewportW, viewportH);
  const QVector3D direction = (farPoint - nearPoint).normalized();
  if (direction.lengthSquared() < 1e-8f) {
    return -1;
  }

  int bestIndex = -1;
  float bestT = std::numeric_limits<float>::max();

  for (int meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex) {
    RhiMesh* mesh = m_meshes[meshIndex];
    if (!mesh || !mesh->isVisible()) {
      continue;
    }
    // GPU 蒙皮：拾取前按需回算一次 CPU 顶点
    if (mesh->isSkinned()) {
      mesh->syncCpuSkinFromBones();
    }
    const CpuMesh& cpu = mesh->cpuMesh();
    if (cpu.bbox.isEmpty()) {
      continue;
    }

    const int triangleCount = cpu.indices.size() / 3;
    for (int tri = 0; tri < triangleCount; ++tri) {
      const quint32 i0 = cpu.indices[tri * 3 + 0];
      const quint32 i1 = cpu.indices[tri * 3 + 1];
      const quint32 i2 = cpu.indices[tri * 3 + 2];
      if (i0 >= static_cast<quint32>(cpu.vertices.size()) ||
          i1 >= static_cast<quint32>(cpu.vertices.size()) ||
          i2 >= static_cast<quint32>(cpu.vertices.size())) {
        continue;
      }

      const QVector3D& v0 = cpu.vertices[i0].position;
      const QVector3D& v1 = cpu.vertices[i1].position;
      const QVector3D& v2 = cpu.vertices[i2].position;

      const QVector3D edge1 = v1 - v0;
      const QVector3D edge2 = v2 - v0;
      const QVector3D h = QVector3D::crossProduct(direction, edge2);
      const float a = QVector3D::dotProduct(edge1, h);
      if (qAbs(a) < 1e-7f) {
        continue;
      }
      const float invA = 1.0f / a;
      const QVector3D s = nearPoint - v0;
      const float u = invA * QVector3D::dotProduct(s, h);
      if (u < 0.0f || u > 1.0f) {
        continue;
      }
      const QVector3D q = QVector3D::crossProduct(s, edge1);
      const float v = invA * QVector3D::dotProduct(direction, q);
      if (v < 0.0f || u + v > 1.0f) {
        continue;
      }
      const float t = invA * QVector3D::dotProduct(edge2, q);
      if (t > 1e-4f && t < bestT) {
        bestT = t;
        bestIndex = meshIndex;
      }
    }
  }

  return bestIndex;
}

void MobileViewerBridge::pickObject(qreal x, qreal y, qreal viewportW,
                                    qreal viewportH) {
  // 拾取依赖投影矩阵，先按当前视口刷新宽高比
  if (viewportW > 0 && viewportH > 0) {
    m_camera.setAspectRatio(float(viewportW / viewportH));
  }
  const int index = pickMesh(QPoint(int(x), int(y)), viewportW, viewportH);
  selectMesh(index);
}

void MobileViewerBridge::selectMesh(int index) {
  if (index == m_selectedMesh) {
    return;
  }
  m_selectedMesh = index;
  emit selectionChanged(index);
  requestFrame();
}

// ── 显示选项 ───────────────────────────────────────────────────────

void MobileViewerBridge::setAxisVisible(bool visible) {
  m_axisVisible = visible;
  requestFrame();
}

void MobileViewerBridge::setGridVisible(bool visible) {
  m_gridVisible = visible;
  requestFrame();
}

void MobileViewerBridge::setCullingEnabled(bool enabled) {
  m_cullEnabled = enabled;
  requestFrame();
}

void MobileViewerBridge::setLodEnabled(bool enabled) {
  m_lodEnabled = enabled;
  requestFrame();
}

void MobileViewerBridge::setNormalLength(qreal length) {
  m_normalLength = qMax(0.0f, float(length));
  requestFrame();
}

void MobileViewerBridge::setClipPlane(qreal x, qreal y, qreal z, qreal w) {
  m_clipPlane = QVector4D(float(x), float(y), float(z), float(w));
  requestFrame();
}

void MobileViewerBridge::setBackgroundColors(qreal topR, qreal topG,
                                             qreal topB, qreal botR,
                                             qreal botG, qreal botB) {
  m_bgTopColor = QVector3D(float(topR), float(topG), float(topB));
  m_bgBotColor = QVector3D(float(botR), float(botG), float(botB));
  requestFrame();
}

// ── 对象列表 ───────────────────────────────────────────────────────

QStringList MobileViewerBridge::meshNames() const {
  QStringList names;
  names.reserve(m_meshes.size());
  for (const RhiMesh* mesh : m_meshes) {
    names.append(mesh ? mesh->name() : QString());
  }
  return names;
}

// ── 动画（逻辑与 RhiWidget 一致）───────────────────────────────────

QString MobileViewerBridge::animationName(int index) const {
  if (index < 0 || index >= m_animationClips.size()) {
    return QString();
  }
  return m_animationClips[index].name;
}

float MobileViewerBridge::animationDuration(int index) const {
  if (index < 0 || index >= m_animationClips.size()) {
    return 0.0f;
  }
  return m_animationClips[index].duration;
}

void MobileViewerBridge::playAnimation(int index) {
  if (m_meshes.isEmpty() || m_animationClips.isEmpty()) {
    return;
  }
  m_animClip = (index >= 0 && index < m_animationClips.size())
                   ? index
                   : 0;
  m_animTime = 0.0f;
  m_animClock.restart();
  m_animPlaying = true;
  applyAnimationTime(m_animTime);
  m_animTimer->start();
  emit animationChanged();
}

void MobileViewerBridge::pauseAnimation() {
  m_animTimer->stop();
  m_animPlaying = false;
  emit animationChanged();
}

void MobileViewerBridge::stopAnimation() {
  stopAnimationInternal();
  emit animationChanged();
  requestFrame();
}

void MobileViewerBridge::stopAnimationInternal() {
  m_animTimer->stop();
  m_animPlaying = false;
  m_animClip = -1;
  m_animTime = 0.0f;
}

void MobileViewerBridge::setAnimationSpeed(qreal speed) {
  m_animSpeed = qBound(0.1f, float(speed), 8.0f);
}

void MobileViewerBridge::advanceAnimation() {
  if (!m_animPlaying || m_animClip < 0 ||
      m_animClip >= m_animationClips.size()) {
    return;
  }
  const float dt = float(m_animClock.restart()) / 1000.0f * m_animSpeed;
  m_animTime += dt;

  const float duration = m_animationClips[m_animClip].duration;
  if (duration > 1e-5f) {
    while (m_animTime >= duration) {
      m_animTime -= duration;
    }
    if (m_animTime < 0.0f) {
      m_animTime += duration;
    }
  } else {
    m_animTime = 0.0f;
  }

  applyAnimationTime(m_animTime);
}

void MobileViewerBridge::applyAnimationTime(float time) {
  if (m_animClip < 0 || m_animClip >= m_animationClips.size()) {
    return;
  }

  const AnimationClip& clip = m_animationClips[m_animClip];
  QHash<QString, const AnimationChannel*> channels;
  channels.reserve(clip.channels.size());
  for (const AnimationChannel& channel : clip.channels) {
    channels.insert(channel.nodeName, &channel);
  }

  bool changed = false;
  for (RhiMesh* mesh : m_meshes) {
    if (!mesh) {
      continue;
    }

    if (mesh->isSkinned()) {
      // 骨骼蒙皮：把动画通道转换为骨骼局部姿态
      QHash<QString, QMatrix4x4> poses;
      poses.reserve(channels.size());
      for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {
        poses.insert(it.key(), it.value()->sample(time));
      }
      mesh->applyBonePoses(poses);
      if (!mesh->gpuSkinEnabled()) {
        changed = true;
      }
      continue;
    }

    const CpuMesh& base = mesh->baseMesh();
    if (base.nodePath.isEmpty()) {
      continue;
    }

    QMatrix4x4 world;
    for (int i = 0; i < base.nodePath.size(); ++i) {
      const QMatrix4x4 rest = (i < base.nodeRestLocals.size())
                                  ? base.nodeRestLocals[i]
                                  : QMatrix4x4();
      auto it = channels.constFind(base.nodePath[i]);
      world *= (it != channels.constEnd()) ? it.value()->sample(time) : rest;
    }

    mesh->setAnimationDelta(world * base.restWorld.inverted());
    changed = true;
  }

  if (changed) {
    m_uploadResetRequested = true;
  }
  requestFrame();
}

void MobileViewerBridge::resetAnimationDeltas() {
  bool changed = false;
  for (RhiMesh* mesh : m_meshes) {
    if (!mesh) {
      continue;
    }
    if (mesh->isSkinned()) {
      mesh->resetSkinPose();
      changed = true;
    } else if (!mesh->animationDelta().isIdentity()) {
      mesh->setAnimationDelta(QMatrix4x4());
      changed = true;
    }
  }
  if (changed) {
    m_uploadResetRequested = true;
  }
}

// ── 截图 ───────────────────────────────────────────────────────────

void MobileViewerBridge::saveScreenshot() {
  if (m_meshes.isEmpty()) {
    emit screenshotFailed(QStringLiteral("当前没有可截图的场景"));
    return;
  }
  m_screenshotRequested = true;
  requestFrame();
}

void MobileViewerBridge::saveScreenshotToFile(QImage image) {
  if (image.isNull()) {
    emit screenshotFailed(QStringLiteral("帧抓取失败"));
    return;
  }
  const QString path = resolveScreenshotPath();
  if (path.isEmpty() || !image.save(path, "png")) {
    emit screenshotFailed(QStringLiteral("无法写入截图文件"));
    return;
  }
  emit screenshotSaved(path);
}

// ── [SG-sync] 渲染线程接口（GUI 线程在 synchronize 时阻塞）──────────

bool MobileViewerBridge::takeUploadResetRequest() {
  const bool requested = m_uploadResetRequested;
  m_uploadResetRequested = false;
  return requested;
}

bool MobileViewerBridge::takeScreenshotRequest() {
  const bool requested = m_screenshotRequested;
  m_screenshotRequested = false;
  return requested;
}

QImage MobileViewerBridge::takePendingDiffuseTexture() {
  QImage image = m_pendingDiffuse;
  m_pendingDiffuse = QImage();
  return image;
}

void MobileViewerBridge::reclaimRetiredMeshes() {
  for (RhiMesh* mesh : m_retired) {
    if (mesh) {
      mesh->setBufferPool(nullptr);
      mesh->releaseGpu();  // GPU 资源在渲染线程释放（此刻 GUI 阻塞）
      delete mesh;
    }
  }
  m_retired.clear();
}

void MobileViewerBridge::deliverScreenshot(const QImage& image) {
  // 从渲染线程投递到 GUI 线程保存（文件 IO 不占用渲染线程）
  QMetaObject::invokeMethod(this, "saveScreenshotToFile",
                            Qt::QueuedConnection, Q_ARG(QImage, image));
}

void MobileViewerBridge::setStatsSnapshot(int objects, int drawn, int culled,
                                           int drawCalls, int triangles,
                                           int lod0, int lod1, int lod2,
                                           float fps) {
  const QString text = QStringLiteral(
                           "对象 %1 · 绘制 %2 · 剔除 %3 · DC %4 · 三角 %5 · "
                           "LOD %6/%7/%8 · %9 FPS")
                           .arg(objects)
                           .arg(drawn)
                           .arg(culled)
                           .arg(drawCalls)
                           .arg(triangles)
                           .arg(lod0)
                           .arg(lod1)
                           .arg(lod2)
                           .arg(int(fps + 0.5f));
  if (text != m_statsText) {  // queued 信号；文本不变则不触发 QML 重绑
    m_statsText = text;
    emit statsTextChanged();
  }
}

// ── 其他 ───────────────────────────────────────────────────────────

void MobileViewerBridge::requestFrame() {
  if (!m_renderPaused) {
    emit frameUpdateRequested();
  }
}

void MobileViewerBridge::setRenderPaused(bool paused) {
  if (paused == m_renderPaused) {
    return;
  }
  m_renderPaused = paused;
  emit renderPausedChanged();
  if (paused) {
    m_animTimer->stop();
  } else if (m_animPlaying) {
    m_animClock.restart();
    m_animTimer->start();
    requestFrame();
  }
}

void MobileViewerBridge::onApplicationStateChanged(
    Qt::ApplicationState state) {
  setRenderPaused(state != Qt::ApplicationActive);
}
