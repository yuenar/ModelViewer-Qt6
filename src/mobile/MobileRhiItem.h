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

#ifndef MODELVIEWER_SRC_MOBILE_MOBILERHIITEM_H_
#define MODELVIEWER_SRC_MOBILE_MOBILERHIITEM_H_

#include <QElapsedTimer>
#include <QImage>
#include <QSize>
#include <QVector>
#include <QtQuick/QQuickRhiItem>

#include "../core/Light.h"
#include "../core/Material.h"
#include "../math/Camera.h"
// 完整定义（而非前置声明）：Q_PROPERTY(MobileViewerBridge*) 的指针
// metatype 在 moc 编译期要求指向完整类型（Qt 6.8+ qtmochelpers 断言）
#include "MobileViewerBridge.h"

class RhiMesh;
class RhiRenderer;
class QRhiReadbackResult;
class QRhiRenderTarget;

/// @brief QML 中的 3D 渲染控件（替代桌面端 RhiWidget）
///
/// 在 QML 中以 `RhiView { bridge: ViewerBridge }` 使用。
/// 真正的渲染逻辑在 MobileRhiItemRenderer（渲染线程）。
class MobileRhiItem : public QQuickRhiItem {
  Q_OBJECT
  Q_PROPERTY(MobileViewerBridge* bridge READ bridge WRITE setBridge NOTIFY
                 bridgeChanged)

 public:
  explicit MobileRhiItem(QQuickItem* parent = nullptr);
  ~MobileRhiItem() override;

  MobileViewerBridge* bridge() const { return m_bridge; }
  void setBridge(MobileViewerBridge* bridge);

 signals:
  void bridgeChanged();

 protected:
  QQuickRhiItemRenderer* createRenderer() override;

 private:
  MobileViewerBridge* m_bridge = nullptr;
};

/// @brief MobileRhiItem 的渲染器（RHI 渲染线程）
///
/// 线程契约（与桌面 QRhiWidget 的关键差异）：
/// - render() 在渲染线程执行，Camera / Light / Material / 网格列表均使用
///   synchronize() 拷贝的快照，不直接访问 GUI 对象；
/// - synchronize() 在 GUI 线程阻塞时调用，是唯一的数据同步点：
///   ① 从 Bridge 拉取场景状态与参数；② 释放退役网格的 GPU 资源；
///   ③ 交付上一帧截图；④ 回写渲染统计（queued 信号）；
/// - Camera 禁止拷贝且 fov 无 setter：两侧 fov 恒为默认 45°，
///   用 setPosition/setTarget/setUp/setProjectionType 逐字段同步。
///
/// 对应扩展文档 §6.1；签名已按 Qt 6.7+ 公开 API 修正：
/// initialize(QRhiCommandBuffer*) + rhi()/renderTarget() 访问器。
class MobileRhiItemRenderer : public QQuickRhiItemRenderer {
 public:
  MobileRhiItemRenderer() = default;
  ~MobileRhiItemRenderer() override;

 protected:
  void initialize(QRhiCommandBuffer* cb) override;
  void synchronize(QQuickRhiItem* item) override;
  void render(QRhiCommandBuffer* cb) override;

 private:
  void initializeRenderer(QRhiCommandBuffer* cb);
  bool grabFrame(QRhiCommandBuffer* cb);
  void flushStats();

  RhiRenderer* m_renderer = nullptr;   ///< 复用桌面渲染核心（零改动）
  MobileViewerBridge* m_bridge = nullptr;
  MobileRhiItem* m_item = nullptr;  ///< 仅用于异步 readback 完成后请求下一帧
  /// 上一次建管线所用的渲染目标（Qt 每次 sync 都会调 initialize()，
  /// 只有目标对象换了才需要重建，避免 Vulkan 下销毁在用管线）
  QRhiRenderTarget* m_lastRt = nullptr;

  // ── 场景状态快照（render() 只读这些）──────────────────────────
  Camera m_camera;      ///< Camera 副本（fov 两侧一致，逐字段同步）
  Light m_light;
  Material m_material;
  QVector<RhiMesh*> m_meshes;
  bool m_renderPaused = false;

  // ── 截图 ──────────────────────────────────────────────────────
  bool m_grabRequested = false;
  bool m_loggedSceneFrame = false;  ///< 首个有模型的帧只打印一次
  int m_debugFrameCount = 0;        ///< 只打印前 3 帧的绘制统计
  bool m_diagReadbackRequested = false;  ///< 一次性颜色纹理回读诊断
  bool m_diagReadbackDone = false;
  // 异步后端（Vulkan）的 readback：数据在帧提交后到达，completed 回调只置位，
  // 图像在下一帧渲染线程内组装（避免在回调里析构自身）
  QRhiReadbackResult* m_readback = nullptr;
  bool m_readbackReady = false;
  QSize m_readbackSize;

  // ── FPS / 统计（约 1 秒窗口）──────────────────────────────────
  QElapsedTimer m_fpsTimer;
  int m_frameCounter = 0;
  float m_fps = 0.0f;

  bool m_pendingInit = false;  ///< initialize 的 cb 为空时推迟到 render()
};

#endif  // MODELVIEWER_SRC_MOBILE_MOBILERHIITEM_H_
