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

#ifndef MODELVIEWER_SRC_CORE_MATERIAL_H_
#define MODELVIEWER_SRC_CORE_MATERIAL_H_

#include <QVector3D>

/// @brief 材质数据结构，用于定义物体的表面光学属性
/// 
/// 该结构体实现了 Phong 光照模型的材质参数，包括环境光反射、
/// 漫反射、镜面反射和光泽度。这些参数决定了物体表面如何与光线交互，
/// 从而产生不同的视觉效果。
/// 
/// Phong 光照模型公式：
/// I = I_ambient + I_diffuse + I_specular
/// 
/// 使用示例：
/// @code
/// Material material;
/// material.ambient = QVector3D(0.1f, 0.1f, 0.1f);    // 低环境光
/// material.diffuse = QVector3D(0.7f, 0.7f, 0.7f);    // 中等漫反射
/// material.specular = QVector3D(1.0f, 1.0f, 1.0f);    // 高镜面反射
/// material.shininess = 64.0f;                          // 较高的光泽度
/// @endcode
struct Material {
  /// @brief 环境光反射系数，表示物体对环境光的反射能力
  /// 
  /// 环境光是场景中的基础光照，模拟光线在环境中的多次反射。
  /// 该值通常设置为较低的值，避免物体看起来过亮。
  /// 
  /// 默认值为 (0.2f, 0.2f, 0.2f)，表示20%的灰色环境光反射。
  /// 
  /// 取值范围：
  /// - (0.0, 0.0, 0.0) - 完全不反射环境光（黑色）
  /// - (1.0, 1.0, 1.0) - 完全反射环境光（白色）
  QVector3D ambient{0.2f, 0.2f, 0.2f};
  
  /// @brief 漫反射系数，表示物体对直接光照的散射反射能力
  /// 
  /// 漫反射是物体表面的主要颜色表现，与光线入射角度相关。
  /// 该值决定了物体的基本颜色和亮度。
  /// 
  /// 默认值为 (0.8f, 0.8f, 0.8f)，表示80%的灰色漫反射。
  /// 
  /// 取值范围：
  /// - (0.0, 0.0, 0.0) - 完全不反射光线（黑色）
  /// - (1.0, 1.0, 1.0) - 完全反射光线（白色）
  /// - 其他值对应不同的物体颜色
  QVector3D diffuse{0.8f, 0.8f, 0.8f};
  
  /// @brief 镜面反射系数，表示物体表面的高光反射能力
  /// 
  /// 镜面反射产生物体表面的高光效果，与观察者位置和光线反射角度相关。
  /// 该值决定了高光的强度和颜色。
  /// 
  /// 默认值为 (0.1f, 0.1f, 0.1f)，表示较弱的白色高光。
  /// 
  /// 取值范围：
  /// - (0.0, 0.0, 0.0) - 无高光效果（哑光表面）
  /// - (1.0, 1.0, 1.0) - 强烈高光（金属表面）
  /// - 有色值可产生彩色高光效果
  QVector3D specular{0.1f, 0.1f, 0.1f};
  
  /// @brief 光泽度，控制镜面反射高光的集中程度
  /// 
  /// 该值决定了高光的大小和锐利程度：
  /// - 较小的值产生大而柔和的高光（哑光表面）
  /// - 较大的值产生小而锐利的高光（光滑表面）
  /// 
  /// 默认值为 32.0f，适合一般的塑料或木材表面。
  /// 
  /// 常用值参考：
  /// - 1.0f - 10.0f：非常粗糙的表面（纸张、布料）
  /// - 10.0f - 50.0f：一般粗糙度表面（塑料、木材）
  /// - 50.0f - 200.0f：较光滑表面（抛光金属）
  /// - 200.0f - 1000.0f：非常光滑的表面（镜子、水面）
  float shininess = 32.0f;
};

#endif  // MODELVIEWER_SRC_CORE_MATERIAL_H_
