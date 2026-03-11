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

#ifndef MODELVIEWER_SRC_MATH_CAMERA_H_
#define MODELVIEWER_SRC_MATH_CAMERA_H_

#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include "BoundingBox.h"

/// @brief 投影类型枚举
/// 
/// 定义了摄像机支持的两种投影模式：
/// - Perspective：透视投影，模拟人眼视觉效果，近大远小
/// - Orthographic：正交投影，保持物体实际比例，常用于工程图纸
enum class ProjectionType {
  Perspective,   ///< 透视投影
  Orthographic    ///< 正交投影
};

class TrackBall;

/// @brief 3D 摄像机类，用于控制视角和投影
/// 
/// 该类实现了 3D 场景中的摄像机控制，包括视图变换、投影变换和用户交互。
/// 支持透视投影和正交投影两种模式，提供旋转、平移、缩放等交互功能。
/// 
/// 主要功能：
/// - 视图矩阵计算（摄像机位置和朝向）
/// - 投影矩阵计算（透视/正交投影）
/// - 用户交互处理（鼠标旋转、平移、缩放）
/// - 自适应视图（自动调整视角以包含整个场景）
/// 
/// 使用示例：
/// @code
/// Camera camera;
/// camera.setAspectRatio(16.0f / 9.0f);
/// camera.fitToView(bbox);
/// 
/// QMatrix4x4 view = camera.viewMatrix();
/// QMatrix4x4 proj = camera.projectionMatrix();
/// @endcode
class Camera {
 public:
  /// @brief 构造函数，初始化摄像机参数
  /// 
  /// 创建默认配置的摄像机：
  /// - 位置：(0, 0, 5)
  /// - 视场角：45度
  /// - 投影类型：透视投影
  Camera();
  
  /// @brief 析构函数，清理资源
  ~Camera();
  
  /// @brief 获取视图矩阵
  /// 
  /// 视图矩阵将世界坐标系中的点转换到摄像机坐标系。
  /// 矩阵包含摄像机的位置、朝向和上方向信息。
  /// 
  /// @return QMatrix4x4 4x4 视图矩阵
  /// 
  /// @note 矩阵遵循右手坐标系约定
  QMatrix4x4 viewMatrix() const;
  
  /// @brief 获取投影矩阵
  /// 
  /// 投影矩阵将摄像机坐标系中的点转换到裁剪坐标系。
  /// 根据当前投影类型（透视/正交）返回相应的投影矩阵。
  /// 
  /// @return QMatrix4x4 4x4 投影矩阵
  /// 
  /// @note 投影矩阵包含近远裁剪面和视场角信息
  QMatrix4x4 projectionMatrix() const;
  
  /// @brief 获取摄像机当前位置
  /// 
  /// @return QVector3D 摄像机在世界坐标系中的位置
  QVector3D position() const { return m_eye; }
  
  /// @brief 设置摄像机宽高比
  /// 
  /// 宽高比用于计算投影矩阵，通常为窗口宽度除以高度。
  /// 
  /// @param[in] aspect 宽高比值（宽度/高度）
  /// 
  /// @note 当窗口大小改变时需要调用此方法
  void setAspectRatio(float aspect);
  
  /// @brief 设置投影类型
  /// 
  /// 在透视投影和正交投影之间切换。
  /// 
  /// @param[in] type 投影类型（Perspective 或 Orthographic）
  void setProjectionType(ProjectionType type);
  
  /// @brief 开始旋转操作
  /// 
  /// 记录旋转起始点，用于后续的旋转计算。
  /// 
  /// @param[in] point 鼠标屏幕坐标
  /// 
  /// @note 通常在鼠标按下事件中调用
  void beginRotate(const QPoint& point);
  
  /// @brief 执行旋转操作
  /// 
  /// 根据鼠标移动计算旋转角度，更新摄像机朝向。
  /// 
  /// @param[in] from 旋转起始点
  /// @param[in] to 旋转结束点
  /// @param[in] viewport 视口大小，用于坐标转换
  /// 
  /// @note 通常在鼠标移动事件中调用
  void rotate(const QPoint& from, const QPoint& to, const QSize& viewport);
  
  /// @brief 执行平移操作
  /// 
  /// 根据鼠标移动方向平移摄像机位置。
  /// 
  /// @param[in] delta 鼠标移动偏移量
  /// 
  /// @note 平移在摄像机视图平面内进行
  void pan(const QPoint& delta);
  
  /// @brief 执行缩放操作
  /// 
  /// 根据缩放因子调整摄像机与目标的距离。
  /// 
  /// @param[in] delta 缩放因子（正值放大，负值缩小）
  /// 
  /// @note 透视投影：调整距离；正交投影：调整缩放比例
  void zoom(float delta);
  
  /// @brief 自动调整视角以包含整个包围盒
  /// 
  /// 计算合适的摄像机位置和参数，使指定的包围盒完全可见。
  /// 
  /// @param[in] bbox 要显示的场景包围盒
  /// 
  /// @note 会自动调整摄像机位置、视场角等参数
  void fitToView(const BoundingBox& bbox);

 private:
  /// @brief 轨迹球控制器，用于处理旋转操作
  TrackBall* m_trackball = nullptr;
  
  /// @brief 旋转中心点，摄像机围绕此点旋转
  QVector3D m_pivot{0, 0, 0};
  
  /// @brief 摄像机位置（眼睛位置）
  QVector3D m_eye{0, 0, 5};
  
  /// @brief 透视投影的视场角（度）
  float m_fov = 45.0f;
  
  /// @brief 近裁剪面距离
  float m_near = 0.1f;
  
  /// @brief 远裁剪面距离
  float m_far = 1000.0f;
  
  /// @brief 视口宽高比
  float m_aspectRatio = 1.0f;
  
  /// @brief 当前投影类型
  ProjectionType m_projType = ProjectionType::Perspective;
  
  /// @brief 上次旋转操作的鼠标位置
  QPoint m_lastRotatePoint;
};

#endif  // MODELVIEWER_SRC_MATH_CAMERA_H_
