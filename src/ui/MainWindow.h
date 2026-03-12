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

#ifndef MODELVIEWER_SRC_UI_MAINWINDOW_H_
#define MODELVIEWER_SRC_UI_MAINWINDOW_H_

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include "RhiWidget.h"

/// @brief 应用程序主窗口类
/// 
/// 该类是 ModelViewer 应用程序的主界面，包含菜单栏、工具栏和中央渲染区域。
/// 提供完整的用户交互功能，包括文件操作、渲染模式切换、设置对话框等。
/// 
/// 主要功能：
/// - 文件操作：打开模型文件
/// - 渲染控制：切换各种渲染模式
/// - 视图控制：投影模式切换、视角适配
/// - 设置对话框：材质和光照参数调整
/// - 工具功能：截图、背景设置
/// 
/// 界面布局：
/// - 顶部：菜单栏和工具栏
/// - 中央：RHI 渲染窗口
/// - 底部：状态栏（可选）
/// 
/// 使用示例：
/// @code
/// MainWindow window;
/// window.show();
/// 
/// // 应用程序事件循环
/// return app.exec();
/// @endcode
class MainWindow : public QMainWindow {
  Q_OBJECT
  
 public:
  /// @brief 构造函数
  /// 
  /// 创建主窗口并初始化所有界面元素。
  /// 
  /// @param[in] parent 父窗口指针，默认为 nullptr
  explicit MainWindow(QWidget* parent = nullptr);
  
  /// @brief 析构函数
  /// 
  /// 使用默认析构函数，Qt 会自动清理子对象。
  ~MainWindow() override = default;
  
 private slots:
  /// @brief 打开文件槽函数
  /// 
  /// 响应"文件->打开"菜单项，显示文件选择对话框。
  /// 支持多种 3D 模型格式的文件选择。
  void onOpenFile();
  
  /// @brief 适配视图槽函数
  /// 
  /// 响应"视图->适配视图"菜单项，自动调整摄像机视角
  /// 以使整个模型在视野内完整显示。
  void onFitView();
  
  /// @brief 切换线框模式槽函数
  /// 
  /// 响应"视图->线框模式"菜单项，在实体渲染和线框渲染之间切换。
  void onToggleWireframe();
  
  /// @brief 切换法线显示槽函数
  /// 
  /// 响应"视图->显示法线"菜单项，显示或隐藏顶点法线可视化。
  void onToggleNormals();
  
  /// @brief 切换面法线显示槽函数
  /// 
  /// 响应"视图->显示面法线"菜单项，显示或隐藏面法线可视化。
  void onToggleFaceNormals();
  
  /// @brief 切换顶点法线显示槽函数
  /// 
  /// 响应"视图->显示顶点法线"菜单项，显示或隐藏增强版顶点法线。
  void onToggleVertexNormals();
  
  /// @brief 切换平面着色槽函数
  /// 
  /// 响应"视图->平面着色"菜单项，在 Phong 着色和平面着色间切换。
  void onToggleFlatShading();
  
  /// @brief 切换 PBR 渲染槽函数
  /// 
  /// 响应"视图->PBR 渲染"菜单项，在传统着色和物理渲染间切换。
  void onTogglePBR();
  
  /// @brief 切换阴影映射槽函数
  /// 
  /// 响应"视图->阴影映射"菜单项，启用或禁用实时阴影效果。
  void onToggleShadowMapping();
  
  /// @brief 切换天空盒槽函数
  /// 
  /// 响应"视图->天空盒"菜单项，显示或隐藏天空盒背景。
  void onToggleSkybox();
  
  /// @brief 切换选择高亮槽函数
  /// 
  /// 响应"视图->选择高亮"菜单项，启用或禁用物体选择高亮效果。
  void onToggleSelection();
  
  /// @brief 切换裁剪平面槽函数
  /// 
  /// 响应"视图->裁剪平面"菜单项，启用或禁用几何体裁剪效果。
  void onToggleClippingPlane();
  
  /// @brief 切换分屏显示槽函数
  /// 
  /// 响应"视图->分屏显示"菜单项，在单屏和分屏对比模式间切换。
  void onToggleSplitScreen();
  
  /// @brief 切换投影模式槽函数
  /// 
  /// 响应"视图->投影模式"菜单项，在透视投影和正交投影间切换。
  void onToggleProjection();
  
  /// @brief 材质设置槽函数
  /// 
  /// 响应"工具->材质设置"菜单项，打开材质参数编辑对话框。
  void onMaterialSettings();
  
  /// @brief 光照设置槽函数
  /// 
  /// 响应"工具->光照设置"菜单项，打开光源参数编辑对话框。
  void onLightSettings();
  
  /// @brief 截图槽函数
  /// 
  /// 响应"工具->截图"菜单项，保存当前渲染画面为图片文件。
  void onScreenshot();
  
  /// @brief 背景设置槽函数
  /// 
  /// 响应"工具->背景设置"菜单项，打开背景颜色编辑对话框。
  void onBackgroundSettings();
  
  /// @brief 视图预设 - 前视图
  void onViewFront();
  
  /// @brief 视图预设 - 后视图
  void onViewBack();
  
  /// @brief 视图预设 - 左视图
  void onViewLeft();
  
  /// @brief 视图预设 - 右视图
  void onViewRight();
  
  /// @brief 视图预设 - 顶视图
  void onViewTop();
  
  /// @brief 视图预设 - 底视图
  void onViewBottom();
  
  /// @brief 视图预设 - 等轴测视图
  void onViewIsometric();
  
  /// @brief 显示模型信息
  void onShowModelInfo();
  
 private:
  /// @brief 设置用户界面
  /// 
  /// 创建和配置主窗口的所有界面元素，包括：
  /// - 中央渲染窗口
  /// - 窗口标题和大小
  /// - 状态栏等
  void setupUI();
  
  /// @brief 设置菜单栏
  /// 
  /// 创建完整的菜单结构：
  /// - 文件菜单：打开、退出等
  /// - 视图菜单：渲染模式、投影切换等
  /// - 工具菜单：设置、截图等
  /// - 帮助菜单：关于等
  void setupMenus();
  
  /// @brief 设置信号连接
  /// 
  /// 连接所有菜单项的触发信号到对应的槽函数。
  void setupConnections();
  
  // === 界面组件 ===
  
  /// @brief RHI 渲染窗口
  /// 
  /// 中央窗口组件，负责实际的 3D 渲染显示。
  /// 继承自 QRhiWidget，支持硬件加速渲染。
  RhiWidget* m_rhiWidget = nullptr;
  
  // === 菜单动作 ===
  
  /// @brief 线框模式菜单动作
  QAction* m_actionWireframe = nullptr;
  
  /// @brief 法线显示菜单动作
  QAction* m_actionNormals = nullptr;
  
  /// @brief 面法线显示菜单动作
  QAction* m_actionFaceNormals = nullptr;
  
  /// @brief 顶点法线显示菜单动作
  QAction* m_actionVertexNormals = nullptr;
  
  /// @brief 平面着色菜单动作
  QAction* m_actionFlatShading = nullptr;
  
  /// @brief PBR 渲染菜单动作
  QAction* m_actionPBR = nullptr;
  
  /// @brief 阴影映射菜单动作
  QAction* m_actionShadowMapping = nullptr;
  
  /// @brief 天空盒菜单动作
  QAction* m_actionSkybox = nullptr;
  
  /// @brief 选择高亮菜单动作
  QAction* m_actionSelection = nullptr;
  
  /// @brief 裁剪平面菜单动作
  QAction* m_actionClippingPlane = nullptr;
  
  /// @brief 分屏显示菜单动作
  QAction* m_actionSplitScreen = nullptr;
  
  /// @brief 正交投影菜单动作
  QAction* m_actionOrtho = nullptr;
};

#endif  // MODELVIEWER_SRC_UI_MAINWINDOW_H_
