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

#ifndef MODELVIEWER_SRC_CORE_LIGHT_H_
#define MODELVIEWER_SRC_CORE_LIGHT_H_

#include <QVector3D>

/// @brief 光源数据结构，用于定义场景中的光照参数
/// 
/// 该结构体定义了点光源的基本属性，包括位置和颜色。
/// 当前实现为简化的点光源模型，可用于 Phong 光照计算。
/// 未来可扩展支持更多光源类型（方向光、聚光灯等）和属性。
/// 
/// 使用示例：
/// @code
/// Light light;
/// light.position = QVector3D(10.0f, 10.0f, 10.0f);
/// light.color = QVector3D(1.0f, 0.8f, 0.6f);  // 暖白色光
/// @endcode
struct Light {
  /// @brief 光源在世界坐标系中的位置
  /// 
  /// 默认值为 (5.0f, 5.0f, 5.0f)，位于场景右上方。
  /// 对于点光源，该位置决定了光照的方向和衰减。
  /// 坐标系使用右手坐标系。
  QVector3D position{5.0f, 5.0f, 5.0f};
  
  /// @brief 光源的颜色，使用 RGB 颜色空间
  /// 
  /// 每个分量的取值范围为 [0.0, 1.0]：
  /// - x 分量：红色强度
  /// - y 分量：绿色强度  
  /// - z 分量：蓝色强度
  /// 
  /// 默认值为 (1.0f, 1.0f, 1.0f)，表示纯白光。
  /// 可以通过调整分量比例来创建不同色温的光源：
  /// - (1.0, 0.8, 0.6) - 暖白光
  /// - (0.6, 0.8, 1.0) - 冷白光
  QVector3D color{1.0f, 1.0f, 1.0f};
};

#endif  // MODELVIEWER_SRC_CORE_LIGHT_H_
