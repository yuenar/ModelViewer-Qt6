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

#ifndef MODELVIEWER_SRC_MATH_TRACKBALL_H_
#define MODELVIEWER_SRC_MATH_TRACKBALL_H_

#include <QQuaternion>
#include <QPointF>
#include <QVector3D>

/// @brief 轨迹球控制器，用于处理 3D 旋转交互
/// 
/// 该类实现了虚拟轨迹球算法，将 2D 鼠标移动转换为 3D 旋转。
/// 通过将屏幕坐标映射到虚拟球面，实现直观的 3D 物体旋转控制。
/// 
/// 算法原理：
/// 1. 将鼠标屏幕坐标映射到单位球面
/// 2. 计算两个球面点之间的旋转轴和角度
/// 3. 使用四元数表示旋转，避免万向锁问题
/// 
/// 主要用途：
/// - 3D 模型查看器的旋转控制
/// - CAD 软件的视角操作
/// - 游戏引擎的摄像机控制
/// 
/// 使用示例：
/// @code
/// TrackBall trackball;
/// 
/// // 鼠标按下
/// trackball.press(mousePos);
/// 
/// // 鼠标移动
/// trackball.press(mousePos);
/// QQuaternion rotation = trackball.rotation();
/// @endcode
class TrackBall {
 public:
  /// @brief 默认构造函数
  /// 
  /// 创建初始状态的轨迹球：
  /// - 旋转：单位四元数（无旋转）
  /// - 状态：未按下
  TrackBall() = default;
  
  /// @brief 处理鼠标按下事件
  /// 
  /// 记录鼠标按下位置，开始旋转操作。
  /// 将屏幕坐标映射到球面并保存为起始点。
  /// 
  /// @param[in] point 鼠标按下的屏幕坐标
  /// 
  /// @note 坐标系原点通常在左上角
  /// @note 调用后会设置按下状态为 true
  void press(const QPointF& point);
  
  /// @brief 处理鼠标移动事件
  /// 
  /// 根据鼠标移动计算新的旋转角度。
  /// 如果鼠标处于按下状态，计算从上次位置到当前位置的旋转。
  /// 
  /// @param[in] point 鼠标移动到的屏幕坐标
  /// 
  /// @note 只有在按下状态下才会计算旋转
  /// @note 旋转会累积到当前旋转四元数中
  void move(const QPointF& point);
  
  /// @brief 获取当前旋转四元数
  /// 
  /// @return QQuaternion 表示当前旋转状态的四元数
  /// 
  /// @note 四元数是单位四元数，可直接用于旋转变换
  /// @note 可以直接应用于 3D 物体或摄像机
  QQuaternion rotation() const;
  
  /// @brief 重置轨迹球状态
  /// 
  /// 将旋转重置为单位四元数（无旋转状态），
  /// 并清除按下状态和最后位置。
  /// 
  /// @note 重置后需要重新调用 press() 开始新的旋转操作
  void reset();

 private:
  /// @brief 将 2D 屏幕坐标映射到 3D 单位球面
  /// 
  /// 这是轨迹球算法的核心函数：
  /// 1. 将屏幕坐标归一化到 [-1, 1] 范围
  /// 2. 计算 Z 坐标使其位于单位球面上
  /// 3. 返回 3D 球面坐标
  /// 
  /// @param[in] p 2D 屏幕坐标点
  /// @return QVector3D 对应的 3D 球面坐标
  /// 
  /// @note 返回的向量是单位向量
  /// @note 如果点在球外，会投影到球面上
  QVector3D mapToSphere(const QPointF& p) const;
  
  /// @brief 当前累积的旋转四元数
  /// 
  /// 表示从初始状态到当前状态的总旋转。
  /// 每次鼠标移动都会更新这个值。
  QQuaternion m_rotation;
  
  /// @brief 上次鼠标位置
  /// 
  /// 用于计算鼠标移动的增量。
  /// 在 press() 和 move() 中更新。
  QPointF m_lastPos;
  
  /// @brief 鼠标按下状态标志
  /// 
  /// true：鼠标当前处于按下状态，可以进行旋转
  /// false：鼠标未按下，忽略移动事件
  bool m_pressed = false;
};

#endif  // MODELVIEWER_SRC_MATH_TRACKBALL_H_
