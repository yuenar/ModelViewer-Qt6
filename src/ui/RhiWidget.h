// Copyright 2024 ModelViewer Project
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
#include <QPoint>
#include "../math/Camera.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../renderer/RhiMesh.h"

class RhiRenderer;

/// @brief 基于 Qt RHI 的 3D 渲染控件
/// 
/// 该类继承自 QRhiWidget，是 ModelViewer 的核心渲染组件。
/// 负责管理渲染器、摄像机、材质、光照等核心功能，
/// 并处理用户的鼠标交互事件。
/// 
/// 主要功能：
/// - 渲染器管理：创建和配置 RhiRenderer
/// - 场景管理：加载和管理 3D 模型
/// - 摄像机控制：旋转、平移、缩放
/// - 材质和光照：实时调整渲染参数
/// - 鼠标交互：轨迹球控制和视图操作
/// 
/// 生命周期：
/// 1. 构造：初始化基本状态
/// 2. initialize()：创建渲染器和资源
/// 3. render()：每帧渲染调用
/// 4. 析构：释放所有 GPU 资源
/// 
/// 使用示例：
/// @code
/// RhiWidget* widget = new RhiWidget(parent);
/// widget->loadModel("model.obj");
/// widget->show();
/// @endcode
class RhiWidget : public QRhiWidget {
  Q_OBJECT
  
 public:
  /// @brief 构造函数
  /// 
  /// @param[in] parent 父窗口指针，默认为 nullptr
  explicit RhiWidget(QWidget* parent = nullptr);
  
  /// @brief 析构函数
  /// 
  /// 安全释放所有 GPU 资源，包括渲染器和网格数据。
  /// 确保 RHI 上下文有效时进行资源清理。
  ~RhiWidget() override;
  
  /// @brief 加载 3D 模型文件
  /// 
  /// 清除当前场景，加载新的 3D 模型。
  /// 支持多种格式，具体格式由 ModelLoader 决定。
  /// 
  /// @param[in] filePath 模型文件的完整路径
  /// 
  /// @note 加载失败时会输出警告信息
  /// @note 会自动调整摄像机视角以适配新模型
  void loadModel(const QString& filePath);
  
  /// @brief 设置渲染模式
  /// 
  /// 切换不同的渲染效果，如 Phong、PBR、线框等。
  /// 
  /// @param[in] mode 渲染模式的整数值
  /// 
  /// @note 具体模式值由 RhiRenderer::RenderMode 定义
  void setRenderMode(int mode);
  
  /// @brief 设置材质参数
  /// 
  /// 更新当前场景的材质属性，包括环境光、漫反射、
  /// 镜面反射和光泽度参数。
  /// 
  /// @param[in] mat 新的材质参数
  /// 
  /// @note 修改后会触发重新渲染
  void setMaterial(const Material& mat);
  
  /// @brief 获取当前材质参数
  /// 
  /// @return Material 当前的材质设置
  Material getMaterial() const { return m_material; }
  
  /// @brief 设置光源参数
  /// 
  /// 更新场景中的光源属性，包括位置和颜色。
  /// 
  /// @param[in] light 新的光源参数
  /// 
  /// @note 修改后会触发重新渲染
  void setLight(const Light& light);
  
  /// @brief 获取当前光源参数
  /// 
  /// @return Light 当前的光源设置
  Light getLight() const { return m_light; }
  
  /// @brief 设置背景颜色
  /// 
  /// 设置渐变背景的顶部和底部颜色。
  /// 
  /// @param[in] topColor 顶部颜色（RGB）
  /// @param[in] botColor 底部颜色（RGB）
  void setBackgroundColors(const QVector3D& topColor, const QVector3D& botColor);
  
  /// @brief 适配视图以包含整个场景
  /// 
  /// 自动调整摄像机的位置和朝向，
  /// 确保场景中的所有物体都在视野范围内。
  void fitToView();
  
  /// @brief 保存当前渲染画面为截图
  /// 
  /// 将当前帧缓冲区内容保存为图片文件。
  /// 
  /// @param[in] filePath 保存文件的完整路径
  /// 
  /// @note 支持的格式由 QImage.save() 决定（PNG、JPG 等）
  /// @note 保存失败时会输出警告信息
  void saveScreenshot(const QString& filePath);
  
  /// @brief 切换投影模式
  /// 
  /// 在透视投影和正交投影之间切换。
  /// 透视投影模拟人眼视觉效果，正交投影保持实际比例。
  void toggleProjection();
  
 protected:
  /// @brief 初始化渲染资源
  /// 
  /// QRhiWidget 的虚函数重写，在控件首次显示时调用。
  /// 创建渲染器实例并初始化所有 GPU 资源。
  /// 
  /// @param[in] cb RHI 命令缓冲区，用于资源创建
  void initialize(QRhiCommandBuffer* cb) override;
  
  /// @brief 渲染函数
  /// 
  /// QRhiWidget 的虚函数重写，每帧调用一次。
  /// 执行实际的 3D 渲染操作。
  /// 
  /// @param[in] cb RHI 命令缓冲区，用于录制渲染命令
  void render(QRhiCommandBuffer* cb) override;
  
  /// @brief 鼠标按下事件处理
  /// 
  /// 记录鼠标按下位置和按钮，开始交互操作。
  /// 
  /// @param[in] e 鼠标事件对象
  void mousePressEvent(QMouseEvent*) override;
  
  /// @brief 鼠标移动事件处理
  /// 
  /// 根据当前按下的按钮执行相应的操作：
  /// - 左键：旋转模型
  /// - 中键：平移视图
  /// 
  /// @param[in] e 鼠标事件对象
  void mouseMoveEvent(QMouseEvent*) override;
  
  /// @brief 鼠标释放事件处理
  /// 
  /// 清除当前按下的按钮状态。
  /// 
  /// @param[in] e 鼠标事件对象
  void mouseReleaseEvent(QMouseEvent*) override;
  
  /// @brief 鼠标滚轮事件处理
  /// 
  /// 执行视图缩放操作。
  /// 
  /// @param[in] e 滚轮事件对象
  void wheelEvent(QWheelEvent*) override;
  
 private:
  /// @brief 渲染器实例
  /// 
  /// 负责实际的 GPU 渲染操作。
  /// 在 initialize() 中创建，在析构函数中释放。
  RhiRenderer*         m_renderer  = nullptr;
  
  /// @brief 摄像机对象
  /// 
  /// 管理 3D 场景的视角、投影和用户交互。
  /// 处理旋转、平移、缩放等操作。
  Camera               m_camera;
  
  /// @brief 当前材质参数
  /// 
  /// Phong 光照模型的材质参数，包括环境光、漫反射、
  /// 镜面反射和光泽度。
  Material             m_material;
  
  /// @brief 当前光源参数
  /// 
  /// 场景中的点光源参数，包括位置和颜色。
  Light                m_light;
  
  /// @brief 场景中的网格列表
  /// 
  /// 存储所有已加载的 3D 模型网格。
  /// 每个网格包含顶点数据、索引数据和 GPU 资源。
  QVector<RhiMesh*>    m_meshes;
  
  /// @brief 场景包围盒
  /// 
  /// 包含场景中所有物体的轴对齐包围盒。
  /// 用于视图适配和碰撞检测。
  BoundingBox          m_sceneBbox;
  
  /// @brief 上次鼠标位置
  /// 
  /// 用于计算鼠标移动的增量。
  /// 在鼠标按下和移动事件中更新。
  QPoint               m_lastMousePos;
  
  /// @brief 当前按下的鼠标按钮
  /// 
  /// 跟踪当前激活的鼠标按钮，用于区分不同的交互模式。
  Qt::MouseButton      m_activeButton = Qt::NoButton;
};

#endif  // MODELVIEWER_SRC_UI_RHIWIDGET_H_
