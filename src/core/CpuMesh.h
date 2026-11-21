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

#ifndef MODELVIEWER_SRC_CORE_CPUMESH_H_
#define MODELVIEWER_SRC_CORE_CPUMESH_H_

#include <QVector3D>
#include <QVector2D>
#include <QString>
#include <QVector>
#include <QImage>
#include <QMatrix4x4>
#include "../math/BoundingBox.h"

/// @brief CPU 端网格数据结构，用于存储 3D 模型的几何数据
/// 
/// 该结构体定义了在 CPU 内存中存储的网格数据格式，包括顶点数据、索引数据和包围盒。
/// 主要用于模型加载器加载模型后的数据存储，以及向 GPU 传输数据前的中间格式。
/// 
/// 使用示例：
/// @code
/// CpuMesh mesh;
/// mesh.vertices.resize(100);
/// mesh.indices.resize(150);
/// mesh.bbox.extend(point);
/// @endcode
struct CpuMesh {
  /// @brief 顶点数据结构
  /// 
  /// 定义了网格中每个顶点的属性，包括位置、法线和纹理坐标。
  /// 所有向量都使用右手坐标系，法线向量假设为单位向量。
  struct Vertex {
    /// @brief 顶点在世界坐标系中的位置
    QVector3D position;
    
    /// @brief 顶点法线向量，用于光照计算
    /// 
    /// 应该是归一化的单位向量，指向顶点朝向。
    /// 用于 Phong 光照模型中的漫反射和镜面反射计算。
    QVector3D normal;
    
    /// @brief 顶点纹理坐标
    /// 
    /// UV 坐标用于纹理映射，U 对应水平方向，V 对应垂直方向。
    /// 坐标范围通常为 [0, 1]，原点 (0,0) 在纹理左下角。
    QVector2D texCoord;
  };

  /// @brief 骨骼蒙皮权重（每顶点最多 4 根骨骼）
  struct SkinVertex {
    quint32 boneIndex[4] = {0, 0, 0, 0};
    float weight[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  };
  
  /// @brief 顶点数组，存储网格的所有顶点数据
  /// 
  /// 每个顶点包含位置、法线和纹理坐标信息。
  /// 顶点的顺序决定了索引数组如何引用它们。
  QVector<Vertex> vertices;
  /// @brief 索引数组，用于定义三角形的拓扑结构
  /// 
  /// 每三个连续的索引值构成一个三角形。
  /// 使用索引可以避免重复存储相同的顶点，提高渲染效率。
  /// 索引值应该是有效的顶点数组下标。
  QVector<quint32> indices;
  
  /// @brief 包围盒，用于快速碰撞检测和视锥剔除
  /// 
  /// 包围盒是一个轴对齐的六面体，完全包含网格的所有顶点。
  /// 用于优化渲染性能，快速判断物体是否在视野内。
  BoundingBox bbox;

  /// @brief 网格名称（来自文件节点或程序化生成）
  QString name;

  /// @brief 基础颜色（来自材质 diffuse，用于加载后初始化材质）
  QVector3D baseColor{0.8f, 0.8f, 0.8f};

  /// @brief 漫反射贴图（来自模型材质，可为空）
  QImage diffuseTexture;

  // ── 动画节点链（用于按节点动画重新计算世界变换）─────────────
  /// @brief 网格所属节点名称
  QString nodeName;

  /// @brief 从根节点到网格所属节点的节点名链
  QVector<QString> nodePath;

  /// @brief 与 nodePath 平行的静止局部变换（rest pose）
  QVector<QMatrix4x4> nodeRestLocals;

  /// @brief 加载时烘焙到顶点上的静止世界变换
  QMatrix4x4 restWorld;

  // ── 骨骼蒙皮（为空表示非蒙皮网格）─────────────────────────
  /// @brief 每顶点的骨骼索引与权重（与 vertices 一一对应）
  QVector<SkinVertex> skinWeights;

  /// @brief 绑定到该网格的骨骼名称
  QVector<QString> boneNames;

  /// @brief 每根骨骼的逆绑定矩阵（mesh 局部空间）
  QVector<QMatrix4x4> boneOffsets;

  /// @brief 每根骨骼的静止世界矩阵 * 逆绑定矩阵（绑定姿态下的蒙皮矩阵）
  QVector<QMatrix4x4> boneBindMatrices;

  /// @brief 每根骨骼从根到骨骼节点的节点名链
  QVector<QVector<QString>> bonePaths;

  /// @brief 与 bonePaths 平行的静止局部变换
  QVector<QVector<QMatrix4x4>> boneRestLocals;

  /// @brief 是否为骨骼蒙皮网格
  bool isSkinned() const {
    return !skinWeights.isEmpty() && !boneNames.isEmpty() &&
           boneNames.size() == boneOffsets.size();
  }
};

#endif  // MODELVIEWER_SRC_CORE_CPUMESH_H_
