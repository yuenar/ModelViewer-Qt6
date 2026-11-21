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

#ifndef MODELVIEWER_SRC_RENDERER_RHIMESH_H_
#define MODELVIEWER_SRC_RENDERER_RHIMESH_H_

#include <rhi/qrhi.h>
#include <QMatrix4x4>
#include <QHash>
#include <QString>
#include <QVector>
#include "../core/CpuMesh.h"
#include "../core/Material.h"

class BufferPool;

/// @brief GPU 端网格数据管理类
///
/// 该类负责管理 GPU 上的网格数据，包括顶点缓冲区和索引缓冲区。
/// 将 CPU 端的 CpuMesh 数据上传到 GPU，并提供渲染所需的缓冲区访问接口。
///
/// 除三角面数据外，还预先在 CPU 端生成两类辅助几何：
/// - 去重后的线框线段索引（用于 Wireframe 模式）
/// - 法线线段顶点（起点 + 方向，用于顶点/面法线可视化）
class RhiMesh {
 public:
  /// @brief 法线线段顶点
  ///
  /// direction 为 0 表示线段起点，为归一化法线表示线段终点；
  /// 着色器按 NormalUBO.length 伸缩，避免重新上传数据。
  struct LineVertex {
    QVector3D position;
    QVector3D direction;
  };

  /// @brief GPU 蒙皮顶点属性（骨骼索引 + 权重）
  struct SkinGpuVertex {
    quint32 boneIndex[4] = {0, 0, 0, 0};
    float weight[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  };

  /// @brief GPU 蒙皮支持的最大骨骼数（与着色器 SkinUBO 保持一致）
  static constexpr int kMaxGpuBones = 128;

  /// @brief 构造函数，从 CPU 网格数据创建 GPU 网格
  ///
  /// @param[in] rhi RHI 设备指针，用于创建 GPU 资源
  /// @param[in] cpu CPU 端的网格数据，包含顶点和索引信息
  RhiMesh(QRhi* rhi, const CpuMesh& cpu);

  /// @brief 析构函数，清理 GPU 资源
  ~RhiMesh();

  /// @brief 设置 RHI 设备指针
  void setRhi(QRhi* rhi) { m_rhi = rhi; }

  /// @brief 设置 GPU 缓冲对象池（可为空，空则自行创建/销毁）
  void setBufferPool(BufferPool* pool) { m_pool = pool; }

  /// @brief 将 CPU 数据上传到 GPU
  ///
  /// @param[in] batch 资源更新批次，用于批量提交更新操作
  void upload(QRhiResourceUpdateBatch* batch);

  /// @brief 释放 GPU 资源（保留 CPU 数据以便重新上传）
  void releaseGpu();

  QRhiBuffer* vertexBuffer() const { return m_vbuf; }
  QRhiBuffer* indexBuffer() const { return m_ibuf; }
  int indexCount() const { return m_indexCount; }
  int vertexCount() const { return m_cpuData.vertices.size(); }

  /// @brief 线框线段索引缓冲（Lines 拓扑，索引对）
  QRhiBuffer* lineIndexBuffer() const { return m_lineIbuf; }
  int lineIndexCount() const { return m_lineIndexCount; }

  /// @brief 距离 LOD 索引缓冲（0 = 原始，1/2 = 粗略级别）
  ///
  /// 采用顶点聚类简化：把顶点量化到不同分辨率的网格，同一网格单元
  /// 只保留一个代表顶点，从而在不新增顶点缓冲的前提下降低三角形数量。
  static constexpr int kLodLevelCount = 2;
  QRhiBuffer* lodIndexBuffer(int level) const {
    return (level >= 1 && level <= kLodLevelCount) ? m_lodIbuf[level - 1]
                                                   : nullptr;
  }
  int lodIndexCount(int level) const {
    return (level >= 1 && level <= kLodLevelCount) ? m_lodIndexCount[level - 1]
                                                   : 0;
  }

  /// @brief 当前网格的包围球
  QVector3D boundingCenter() const { return m_center; }
  float boundingRadius() const { return m_radius; }

  /// @brief 顶点法线线段（Lines 拓扑，非索引绘制）
  QRhiBuffer* vertexNormalBuffer() const { return m_vnormalVbuf; }
  int vertexNormalVertexCount() const { return m_vertexNormalLines.size(); }

  /// @brief 面法线线段（Lines 拓扑，非索引绘制）
  QRhiBuffer* faceNormalBuffer() const { return m_fnormalVbuf; }
  int faceNormalVertexCount() const { return m_faceNormalLines.size(); }

  /// @brief 访问 CPU 端数据（用于拾取、统计等）
  const CpuMesh& cpuMesh() const { return m_cpuData; }

  /// @brief 未经变换的原始 CPU 数据（用于复制对象/重新变换）
  const CpuMesh& baseMesh() const { return m_baseData; }

  // ── 对象属性 ─────────────────────────────────────────────
  void setName(const QString& name) { m_name = name; }
  QString name() const { return m_name; }

  void setVisible(bool visible) { m_visible = visible; }
  bool isVisible() const { return m_visible; }

  /// @brief 设置对象变换（在基础几何上重新烘焙并重建线段数据）
  ///
  /// 变换直接作用于 CPU 顶点/法线并重新上传，因此拾取、包围盒、
  /// 阴影等所有下游逻辑都会自动跟随。
  void setTransform(const QMatrix4x4& transform);
  QMatrix4x4 transform() const { return m_transform; }
  void resetTransform() { setTransform(QMatrix4x4()); }

  /// @brief 设置动画产生的附加变换（在用户变换之前应用）
  void setAnimationDelta(const QMatrix4x4& delta);
  QMatrix4x4 animationDelta() const { return m_animDelta; }

  /// @brief 是否为骨骼蒙皮网格
  bool isSkinned() const { return m_skinned; }

  /// @brief 是否使用 GPU 蒙皮（骨骼数在限制内）
  bool gpuSkinEnabled() const { return m_gpuSkin; }

  /// @brief GPU 蒙皮的骨骼矩阵（骨骼空间 -> 模型空间）
  const QVector<QMatrix4x4>& boneMatrices() const { return m_boneMatrices; }

  /// @brief 作用于蒙皮结果的用户对象变换
  QMatrix4x4 objectMatrix() const { return m_transform; }

  /// @brief 蒙皮顶点属性缓冲；非蒙皮网格返回普通顶点缓冲（着色器不会读取）
  QRhiBuffer* skinVertexBuffer() const {
    return m_skinVBuf ? m_skinVBuf : m_vbuf;
  }

  /// @brief 应用骨骼姿态（骨骼名 -> 局部变换），并重新计算蒙皮顶点
  void applyBonePoses(const QHash<QString, QMatrix4x4>& poses);

  /// @brief 恢复绑定姿态
  void resetSkinPose();

  /// @brief 按需用当前骨骼矩阵回算 CPU 顶点
  ///
  /// GPU 蒙皮时顶点缓冲保持局部坐标；拾取、剖面封盖等 CPU 侧逻辑
  /// 在需要时调用该方法做一次等价“回读”，不影响 GPU 数据。
  void syncCpuSkinFromBones();

  /// @brief 复制该对象（共享原始几何，独立的 GPU 资源与变换）
  RhiMesh* clone() const;

  void setMaterial(const Material& material) { m_material = material; }
  const Material& material() const { return m_material; }

 private:
  void buildLineIndices();
  void buildNormalLines();
  void buildLodIndices(int level, int cells);
  void rebuildDerivedData();
  void rebuildCpuData(bool updateDerived = true);
  void initSkinData(const CpuMesh& cpu);
  void updateBoneMatrices(const QHash<QString, QMatrix4x4>* poses);
  void skinVertices();

  QRhiBuffer* createBuffer(QRhiResourceUpdateBatch* batch,
                           QRhiBuffer::UsageFlag usage, const void* data,
                           quint32 size, quint32* capacity);
  void destroyBuffer(QRhiBuffer** buffer, QRhiBuffer::UsageFlag usage,
                     quint32 capacity);

  QRhi* m_rhi = nullptr;
  BufferPool* m_pool = nullptr;
  QRhiBuffer* m_vbuf = nullptr;
  QRhiBuffer* m_ibuf = nullptr;
  QRhiBuffer* m_lineIbuf = nullptr;
  QRhiBuffer* m_vnormalVbuf = nullptr;
  QRhiBuffer* m_fnormalVbuf = nullptr;
  QRhiBuffer* m_skinVBuf = nullptr;
  QRhiBuffer* m_lodIbuf[kLodLevelCount] = {nullptr, nullptr};

  quint32 m_vbufCapacity = 0;
  quint32 m_ibufCapacity = 0;
  quint32 m_lineIbufCapacity = 0;
  quint32 m_vnormalCapacity = 0;
  quint32 m_fnormalCapacity = 0;
  quint32 m_skinVCapacity = 0;
  quint32 m_lodIbufCapacity[kLodLevelCount] = {0, 0};

  CpuMesh m_baseData;   ///< 未变换的原始数据
  CpuMesh m_cpuData;    ///< 当前（已变换）数据
  QVector<quint32> m_lineIndices;
  QVector<quint32> m_lodIndices[kLodLevelCount];
  QVector<LineVertex> m_vertexNormalLines;
  QVector<LineVertex> m_faceNormalLines;

  QString m_name;
  bool m_visible = true;
  QMatrix4x4 m_userTransform;   ///< 用户对象变换
  QMatrix4x4 m_animDelta;       ///< 动画附加变换
  QMatrix4x4 m_transform;       ///< 实际烘焙到顶点的组合变换
  Material m_material;

  QVector3D m_center;
  float m_radius = 0.0f;

  // ── 骨骼蒙皮数据 ─────────────────────────────────────────
  bool m_skinned = false;
  bool m_gpuSkin = false;
  QVector<CpuMesh::SkinVertex> m_skinWeights;
  QVector<QString> m_boneNames;
  QVector<QMatrix4x4> m_boneOffsets;
  QVector<QMatrix4x4> m_boneBindMatrices;
  QVector<QVector<QString>> m_bonePaths;
  QVector<QVector<QMatrix4x4>> m_boneRestLocals;
  QVector<QMatrix4x4> m_boneMatrices;
  QVector<QMatrix4x4> m_boneNormalMatrices;

  int m_indexCount = 0;
  int m_lineIndexCount = 0;
  int m_lodIndexCount[kLodLevelCount] = {0, 0};
  bool m_uploaded = false;
};

#endif  // MODELVIEWER_SRC_RENDERER_RHIMESH_H_
