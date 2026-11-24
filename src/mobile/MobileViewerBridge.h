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

#ifndef MODELVIEWER_SRC_MOBILE_MOBILEVIEWERBRIDGE_H_
#define MODELVIEWER_SRC_MOBILE_MOBILEVIEWERBRIDGE_H_

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QVector3D>
#include <QVector4D>

#include "../core/Animation.h"
#include "../core/Light.h"
#include "../core/Material.h"
#include "../math/BoundingBox.h"
#include "../math/Camera.h"

class RhiMesh;

/// @brief 移动端 QML ↔ C++ 主桥接对象（QML 单例，GUI 线程）
///
/// 职责（对照移动端扩展文档 §8.3，按实际代码 API 调整）：
/// - 持有 Camera / Light / Material / RhiMesh 集合 / 动画剪辑等场景状态；
/// - 手势 → Camera 的换算（桌面 RhiWidget 鼠标事件同款语义，
///   Camera 内部自带 TrackBall，因此不再单独实现 MobileGestureHandler）；
/// - 模型异步加载（QtConcurrent 线程池 + 进度信号）；
/// - 触摸拾取（纯 CPU 三角形级射线求交，逻辑取自 RhiWidget::pickMesh）；
/// - 截图请求（实际抓帧由 MobileRhiItemRenderer 在渲染线程完成）。
///
/// 线程模型：本对象只在 GUI 线程访问；唯一例外是
/// MobileRhiItemRenderer::synchronize()（GUI 线程阻塞的同步点）会在
/// 渲染线程调用标注 [SG-sync] 的只读/一次性方法。
class MobileViewerBridge : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
  Q_PROPERTY(QString modelLabel READ modelLabel NOTIFY modelLabelChanged)
  Q_PROPERTY(QString statsText READ statsText NOTIFY statsTextChanged)
  Q_PROPERTY(bool renderPaused READ isRenderPaused NOTIFY renderPausedChanged)
  Q_PROPERTY(bool animationPlaying READ isAnimationPlaying NOTIFY animationChanged)
  Q_PROPERTY(int animationCount READ animationCount NOTIFY animationChanged)
  Q_PROPERTY(int meshCount READ meshCount NOTIFY meshesChanged)

 public:
  explicit MobileViewerBridge(QObject* parent = nullptr);
  ~MobileViewerBridge() override;

  // ── Q_PROPERTY 读取器 ─────────────────────────────────────────
  bool isLoading() const { return m_loading; }
  QString modelLabel() const { return m_modelLabel; }
  QString statsText() const { return m_statsText; }
  bool isAnimationPlaying() const { return m_animPlaying; }
  int meshCount() const { return m_meshes.size(); }

  // ── QML 可调用接口 ─────────────────────────────────────────────
 public slots:
  // 模型加载（Android 上为 SAF 拷贝到 cache 后的本地路径）
  void loadModel(const QString& path);
  /// @brief 生成参数曲面演示模型（0 平面 / 1 球 / 2 环 / 3 圆柱 / 4 圆锥）
  void createSurface(int type);

  // 渲染模式 / 显示风格（枚举值与 RhiRenderer::RenderMode、
  // DisplayStyle 一一对应，QML 面板负责展示名称）
  void setRenderMode(int mode);
  void setDisplayStyle(int style);
  void setMultiView(bool enabled);
  void setOrthographic(bool ortho);

  // 视图控制
  void resetView();           // 相机复位 + 适配
  void fitView();             // 适配场景包围盒
  void applyViewPreset(int preset);  // 0 前 … 6 等轴测 … 8 三轴测

  // 手势（语义对齐桌面 RhiWidget 的鼠标处理）
  /// @brief 单指按下：记录旋转起点（对应 Camera::beginRotate）
  void beginRotateGesture(qreal x, qreal y);
  /// @brief 单指拖动：绝对坐标 + 视口尺寸（对应 Camera::rotate）
  void rotateAt(qreal x, qreal y, qreal viewportW, qreal viewportH);
  /// @brief 双指拖动平移（对应 Camera::pan）
  void panGesture(qreal dx, qreal dy);
  /// @brief 双指捏合缩放，正数放大（对应 Camera::zoom）
  void zoomGesture(qreal scaleDelta);
  /// @brief 长按拾取（射线三角形求交，选中对象高亮）
  void pickObject(qreal x, qreal y, qreal viewportW, qreal viewportH);

  // 显示选项
  void setAxisVisible(bool visible);
  void setGridVisible(bool visible);
  void setCullingEnabled(bool enabled);
  void setLodEnabled(bool enabled);
  void setNormalLength(qreal length);
  void setClipPlane(qreal x, qreal y, qreal z, qreal w);
  void setBackgroundColors(qreal topR, qreal topG, qreal topB,
                           qreal botR, qreal botG, qreal botB);

  // 对象
  QStringList meshNames() const;
  void selectMesh(int index);

  // 动画（节点关键帧）
  int animationCount() const { return m_animationClips.size(); }
  QString animationName(int index) const;
  float animationDuration(int index) const;
  void playAnimation(int index);
  void pauseAnimation();
  void stopAnimation();
  void setAnimationSpeed(qreal speed);

  // 截图：请求 MobileRhiItemRenderer 抓帧，结果经 deliverScreenshot 保存
  void saveScreenshot();

  /// @brief 请求重绘一帧（手势 / 参数变化后由内部自动调用）
  void requestFrame();

 signals:
  void loadingChanged();
  void modelLabelChanged();
  void statsTextChanged();
  void renderPausedChanged();
  void animationChanged();
  void meshesChanged();

  /// @brief 触发 MobileRhiItem::update()（GUI 线程信号）
  void frameUpdateRequested();

  void loadStarted();
  void loadProgress(qreal progress);
  void modelLoaded(const QString& label, int meshCount, int triangles);
  void loadError(const QString& message);
  void renderModeChanged(int mode);
  void selectionChanged(int index);
  void screenshotSaved(const QString& path);
  void screenshotFailed(const QString& message);

 public:
  // ── [SG-sync] 渲染线程在 synchronize() 中读取的场景状态 ──────────
  const Camera& camera() const { return m_camera; }
  const Light& light() const { return m_light; }
  const Material& material() const { return m_material; }
  const QVector<RhiMesh*>& meshes() const { return m_meshes; }
  QImage takePendingDiffuseTexture();

  int renderMode() const { return m_renderMode; }
  int displayStyle() const { return m_displayStyle; }
  bool multiView() const { return m_multiView; }
  bool axisVisible() const { return m_axisVisible; }
  bool gridVisible() const { return m_gridVisible; }
  bool cullingEnabled() const { return m_cullEnabled; }
  bool lodEnabled() const { return m_lodEnabled; }
  bool isRenderPaused() const { return m_renderPaused; }
  int selectedMesh() const { return m_selectedMesh; }
  float normalLength() const { return m_normalLength; }
  QVector4D clipPlane() const { return m_clipPlane; }
  QVector3D backgroundTopColor() const { return m_bgTopColor; }
  QVector3D backgroundBottomColor() const { return m_bgBotColor; }

  /// @brief [SG-sync] 一次性读取并清除"重置网格上传"请求
  bool takeUploadResetRequest();
  /// @brief [SG-sync] 一次性读取并清除截图请求
  bool takeScreenshotRequest();
  /// @brief [SG-sync] 在渲染线程释放并删除退役网格（GUI 线程此刻阻塞）
  void reclaimRetiredMeshes();
  /// @brief [SG-sync] 渲染线程交付截图（转投 GUI 线程保存）
  void deliverScreenshot(const QImage& image);
  /// @brief [SG-sync] 渲染线程回写统计（跨线程信号自动队列化到 GUI 线程）
  void setStatsSnapshot(int objects, int drawn, int culled, int drawCalls,
                        int triangles, int lod0, int lod1, int lod2,
                        float fps);

 private slots:
  void saveScreenshotToFile(QImage image);
  void advanceAnimation();
  void onApplicationStateChanged(Qt::ApplicationState state);

 private:
  void setMeshesInternal(QVector<CpuMesh> meshes,
                         const QString& label,
                         QVector<AnimationClip> animations);
  void applyAnimationTime(float time);
  void resetAnimationDeltas();
  void stopAnimationInternal();
  void setRenderPaused(bool paused);

  QVector3D unproject(const QPoint& pos, float ndcZ,
                      qreal viewportW, qreal viewportH) const;
  int pickMesh(const QPoint& pos, qreal viewportW, qreal viewportH) const;

  // ── 场景状态（GUI 线程）────────────────────────────────────────
  Camera m_camera;
  Light m_light;
  Material m_material;
  QVector<RhiMesh*> m_meshes;
  QVector<RhiMesh*> m_retired;  ///< 待渲染线程释放 GPU 资源后删除
  BoundingBox m_sceneBbox;
  QString m_modelLabel;
  QImage m_pendingDiffuse;  ///< 加载后待上传的漫反射贴图

  // ── 渲染参数（GUI 线程写，[SG-sync] 渲染线程读）─────────────────
  int m_renderMode = 0;
  int m_displayStyle = 0;
  bool m_multiView = false;
  bool m_axisVisible = false;
  bool m_gridVisible = false;
  bool m_cullEnabled = true;
  bool m_lodEnabled = true;
  bool m_renderPaused = false;
  int m_selectedMesh = -1;
  float m_normalLength = 0.1f;
  QVector4D m_clipPlane{-1.0f, 0.0f, 0.0f, 0.0f};
  QVector3D m_bgTopColor{0.2f, 0.2f, 0.3f};
  QVector3D m_bgBotColor{0.1f, 0.1f, 0.15f};

  // ── 状态标志 ──────────────────────────────────────────────────
  bool m_loading = false;
  bool m_uploadResetRequested = false;
  bool m_screenshotRequested = false;
  QString m_statsText;

  // ── 动画（GUI 线程；逻辑与 RhiWidget 保持一致）──────────────────
  QVector<AnimationClip> m_animationClips;
  QTimer* m_animTimer = nullptr;
  QElapsedTimer m_animClock;
  int m_animClip = -1;
  float m_animTime = 0.0f;
  float m_animSpeed = 1.0f;
  bool m_animPlaying = false;

  // ── 最近一次鼠标/手指位置（手势换算）──────────────────────────
  QPoint m_lastRotatePoint;
  bool m_rotating = false;
};

#endif  // MODELVIEWER_SRC_MOBILE_MOBILEVIEWERBRIDGE_H_
