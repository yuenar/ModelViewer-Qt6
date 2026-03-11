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

#ifndef MODELVIEWER_SRC_MATH_BOUNDINGBOX_H_
#define MODELVIEWER_SRC_MATH_BOUNDINGBOX_H_

#include <QVector3D>
#include <cmath>

/// @brief 轴对齐包围盒（AABB - Axis-Aligned Bounding Box）类
/// 
/// 该类实现了一个轴对齐的包围盒，用于快速的空间查询和碰撞检测。
/// 包围盒是一个与坐标轴对齐的长方体，完全包含指定的点集或物体。
/// 
/// 主要用途：
/// - 视锥剔除：快速判断物体是否在视野内
/// - 碰撞检测：检测两个物体是否可能相交
/// - 空间分割：用于空间数据结构（如八叉树、BVH）
/// - 性能优化：避免对不可见物体进行详细计算
/// 
/// 使用示例：
/// @code
/// BoundingBox bbox;
/// bbox.expand(QVector3D(1, 2, 3));
/// bbox.expand(QVector3D(-1, -2, -3));
/// 
/// QVector3D center = bbox.center();
/// float radius = bbox.radius();
/// @endcode
class BoundingBox {
 public:
  /// @brief 默认构造函数，创建一个空的包围盒
  /// 
  /// 空包围盒的 min 点设置为 (+∞, +∞, +∞)，
  /// max 点设置为 (-∞, -∞, -∞)，这样第一个点可以正确初始化包围盒。
  BoundingBox() = default;

  /// @brief 重置包围盒为空状态
  /// 
  /// 将包围盒恢复到初始的空状态，min 和 max 点被设置为极值。
  /// 调用后，isEmpty() 将返回 true，需要重新添加点来构建包围盒。
  void reset();

  /// @brief 扩展包围盒以包含指定的点
  /// 
  /// 如果包围盒为空，该点将成为包围盒的初始边界。
  /// 否则，调整 min 和 max 点以确保包含该点。
  /// 
  /// @param[in] point 要包含的 3D 点
  /// 
  /// @note 该方法会自动处理空包围盒的情况
  /// @note 时间复杂度：O(1)
  void expand(const QVector3D& point);

  /// @brief 扩展包围盒以包含另一个包围盒
  /// 
  /// 计算两个包围盒的并集，结果是一个包含两个原始包围盒的新包围盒。
  /// 
  /// @param[in] other 要合并的另一个包围盒
  /// 
  /// @note 如果任一包围盒为空，结果将是另一个包围盒
  /// @note 如果两个包围盒都为空，结果仍为空
  void expand(const BoundingBox& other);

  /// @brief 获取包围盒的中心点
  /// 
  /// @return QVector3D 包围盒的几何中心坐标
  /// 
  /// @note 对于空包围盒，返回 (0, 0, 0)
  QVector3D center() const;

  /// @brief 获取包围盒的尺寸（长、宽、高）
  /// 
  /// @return QVector3D 包围盒在 X、Y、Z 轴上的长度
  /// 
  /// @note 对于空包围盒，返回 (0, 0, 0)
  QVector3D size() const;

  /// @brief 获取包围盒的外接球半径
  /// 
  /// 计算从包围盒中心到最远顶点的距离，即包围盒的外接球半径。
  /// 
  /// @return float 外接球半径
  /// 
  /// @note 对于空包围盒，返回 0.0f
  /// @note 用于简化的碰撞检测和距离计算
  float radius() const;

  /// @brief 检查包围盒是否为空
  /// 
  /// @return bool 如果包围盒为空返回 true，否则返回 false
  /// 
  /// @note 空包围盒表示还没有添加任何点
  bool isEmpty() const;

 private:
  /// @brief 包围盒的最小角点
  /// 
  /// 初始化为 (+∞, +∞, +∞)，确保第一个点可以正确更新该值。
  /// 在扩展过程中，该值始终不大于任何包围盒内的点。
  QVector3D m_min{FLT_MAX, FLT_MAX, FLT_MAX};

  /// @brief 包围盒的最大角点
  /// 
  /// 初始化为 (-∞, -∞, -∞)，确保第一个点可以正确更新该值。
  /// 在扩展过程中，该值始终不小于任何包围盒内的点。
  QVector3D m_max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
};

#endif  // MODELVIEWER_SRC_MATH_BOUNDINGBOX_H_
