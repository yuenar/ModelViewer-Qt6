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

#ifndef MODELVIEWER_SRC_UI_RHIWIDGET_H_
#define MODELVIEWER_SRC_UI_RHIWIDGET_H_

#include <QRhiWidget>
#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QPoint>
#include <QTimer>
#include <QVector4D>
#include "../math/Camera.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../core/Animation.h"
#include "../renderer/RhiMesh.h"

class RhiRenderer;
class QWidget;

/// @brief 交互工具
enum class ToolMode {
  Rotate = 0,
  Pan = 1,
  Zoom = 2,
  Select = 3
};

/// @brief 基于 Qt RHI 的 3D 渲染控件
class RhiWidget : public QRhiWidget {
  Q_OBJECT

 public:
  explicit RhiWidget(QWidget* parent = nullptr);
  ~RhiWidget() override;

  void loadModel(const QString& filePath);
  void setCPUMeshes(const QVector<CpuMesh>& meshes, const QString& label);

  void setRenderMode(int mode);
  int renderMode() const { return m_renderMode; }
  void setDisplayStyle(int style);
  int displayStyle() const { return m_displayStyle; }
  void setMultiView(bool enabled);
  bool isMultiView() const { return m_multiView; }

  void setMaterial(const Material& mat);
  Material getMaterial() const { return m_material; }
  void setMeshMaterial(int index, const Material& mat);
  Material meshMaterial(int index) const;
  void setLight(const Light& light);
  Light getLight() const { return m_light; }

  void setBackgroundColors(const QVector3D& topColor, const QVector3D& botColor);
  QVector3D backgroundTopColor() const { return m_bgTopColor; }
  QVector3D backgroundBottomColor() const { return m_bgBotColor; }
  void fitToView();
  void applyViewPreset(int preset);

  void applyMaterialPreset(const QString& name);

  void saveScreenshot(const QString& filePath);
  bool saveScreenshotEx(const QString& filePath, const QString& format = "png",
                        int quality = 95);
  bool saveScreenshotScaled(const QString& filePath, int width, int height);
  QString getModelStatistics() const;

  void setProjectionType(ProjectionType type);
  void toggleProjection();
  bool isOrthographic() const;

  void setNormalLength(float length);
  void setClippingPlane(const QVector4D& plane);
  QVector4D clippingPlane() const { return m_clipPlane; }

  void selectMesh(int index);
  int selectedMesh() const { return m_selectedMesh; }

  void setAxisVisible(bool visible);
  bool isAxisVisible() const { return m_axisVisible; }
  void setGridVisible(bool visible);
  bool isGridVisible() const { return m_gridVisible; }

  void setToolMode(ToolMode mode);
  ToolMode toolMode() const { return m_toolMode; }

  // ── 对象列表接口 ─────────────────────────────────────────
  int meshCount() const { return m_meshes.size(); }
  QString meshName(int index) const;
  bool isMeshVisible(int index) const;
  void setMeshVisible(int index, bool visible);
  QMatrix4x4 meshTransform(int index) const;
  void setMeshTransform(int index, const QMatrix4x4& transform);
  void resetMeshTransform(int index);
  void showAllMeshes(bool visible);
  void swapMeshVisibility();

  /// @brief 模型编辑：删除 / 复制 / 重命名
  bool removeMeshAt(int index);
  int duplicateMeshAt(int index);
  bool renameMeshAt(int index, const QString& name);

  BoundingBox sceneBoundingBox() const { return m_sceneBbox; }
  QString sourceLabel() const { return m_sourceLabel; }
  QVector<CpuMesh> cpuMeshes() const;

  float fps() const { return m_fps; }
  QString renderModeName() const;
  QString displayStyleName() const;

  /// @brief 单帧渲染统计摘要（绘制调用/剔除/LOD/缓冲池）
  QString renderStatsText() const;

  /// @brief 视锥剔除与距离 LOD 开关
  void setCullingEnabled(bool enabled);
  bool isCullingEnabled() const { return m_cullEnabled; }
  void setLodEnabled(bool enabled);
  bool isLodEnabled() const { return m_lodEnabled; }

  /// @brief MSAA 采样数（1 关闭，常用 2/4）
  void setMsaaSampleCount(int samples);
  int msaaSampleCount() const { return sampleCount(); }

  /// @brief 后处理开关（FXAA / 深度 AO），与 MSAA 互斥
  void setPostProcessing(bool fxaa, bool ao);
  bool isFxaaEnabled() const { return m_fxaaEnabled; }
  bool isAoEnabled() const { return m_aoEnabled; }

  // ── 动画（节点关键帧）────────────────────────────────────
  void setAnimationClips(const QVector<AnimationClip>& clips);
  int animationCount() const { return m_animationClips.size(); }
  QString animationName(int index) const;
  float animationDuration(int index) const;
  void playAnimation(int index);
  void pauseAnimation();
  void stopAnimation();
  bool isAnimationPlaying() const { return m_animPlaying; }
  int currentAnimation() const { return m_animClip; }
  float animationTime() const { return m_animTime; }
  void setAnimationTime(float seconds);
  void setAnimationSpeed(float speed) { m_animSpeed = speed; }

  Camera& getCamera() { return m_camera; }
  const Camera& getCamera() const { return m_camera; }

  // ── 视图历史 / 过渡动画 ──────────────────────────────────
  struct ViewState {
    QVector3D eye;
    QVector3D target;
    QVector3D up;
  };
  void recordViewState();
  bool canGoBack() const { return m_viewHistoryIndex > 0; }
  bool canGoForward() const {
    return m_viewHistoryIndex >= 0 &&
           m_viewHistoryIndex + 1 < m_viewHistory.size();
  }
  void goBack();
  void goForward();
  int viewHistorySize() const { return m_viewHistory.size(); }

 signals:
  void projectionChanged(bool orthographic);
  void meshesChanged();
  void viewHistoryChanged();

 protected:
  void initialize(QRhiCommandBuffer* cb) override;
  void render(QRhiCommandBuffer* cb) override;
  void releaseResources() override;

  void resizeEvent(QResizeEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;

 private:
  int pickMesh(const QPoint& pos) const;
  QVector3D unproject(const QPoint& pos, float ndcZ) const;
  void updateCursorForTool();
  void updateFps();
  void applyAnimationTime(float time);
  void advanceAnimation();
  void resetAnimationDeltas();
  ViewState currentViewState() const;
  void animateToViewState(const ViewState& state);
  bool viewStateNear(const ViewState& a, const ViewState& b) const;

  RhiRenderer* m_renderer = nullptr;
  Camera m_camera;
  Material m_material;
  Light m_light;
  QVector<RhiMesh*> m_meshes;
  BoundingBox m_sceneBbox;
  QString m_sourceLabel;

  QPoint m_lastMousePos;
  QPoint m_pressPos;
  Qt::MouseButton m_activeButton = Qt::NoButton;
  bool m_dragged = false;

  int m_selectedMesh = -1;
  int m_renderMode = 0;
  int m_displayStyle = 0;
  bool m_multiView = false;
  QVector4D m_clipPlane{-1.0f, 0.0f, 0.0f, 0.0f};
  bool m_axisVisible = false;
  bool m_gridVisible = false;
  bool m_cullEnabled = true;
  bool m_lodEnabled = true;
  bool m_fxaaEnabled = false;
  bool m_aoEnabled = false;
  ToolMode m_toolMode = ToolMode::Rotate;
  QVector3D m_bgTopColor{0.2f, 0.2f, 0.3f};
  QVector3D m_bgBotColor{0.1f, 0.1f, 0.15f};
  QImage m_pendingDiffuseTexture;

  QWidget* m_hud = nullptr;
  QElapsedTimer m_fpsTimer;
  int m_frameCounter = 0;
  float m_fps = 0.0f;

  // ── 动画状态 ─────────────────────────────────────────────
  QVector<AnimationClip> m_animationClips;
  QTimer* m_animTimer = nullptr;
  QElapsedTimer m_animClock;
  int m_animClip = -1;
  float m_animTime = 0.0f;
  float m_animSpeed = 1.0f;
  bool m_animPlaying = false;

  // ── 视图历史状态 ─────────────────────────────────────────
  QVector<ViewState> m_viewHistory;
  int m_viewHistoryIndex = -1;
  QTimer* m_viewAnimTimer = nullptr;
  QTimer* m_viewRecordTimer = nullptr;
  QElapsedTimer m_viewAnimClock;
  ViewState m_viewAnimFrom;
  ViewState m_viewAnimTo;
  bool m_viewAnimating = false;
};

#endif  // MODELVIEWER_SRC_UI_RHIWIDGET_H_
