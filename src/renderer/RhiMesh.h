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
#include <QString>
#include "../core/CpuMesh.h"

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

  /// @brief 构造函数，从 CPU 网格数据创建 GPU 网格
  ///
  /// @param[in] rhi RHI 设备指针，用于创建 GPU 资源
  /// @param[in] cpu CPU 端的网格数据，包含顶点和索引信息
  RhiMesh(QRhi* rhi, const CpuMesh& cpu);

  /// @brief 析构函数，清理 GPU 资源
  ~RhiMesh();

  /// @brief 设置 RHI 设备指针
  void setRhi(QRhi* rhi) { m_rhi = rhi; }

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

  /// @brief 顶点法线线段（Lines 拓扑，非索引绘制）
  QRhiBuffer* vertexNormalBuffer() const { return m_vnormalVbuf; }
  int vertexNormalVertexCount() const { return m_vertexNormalLines.size(); }

  /// @brief 面法线线段（Lines 拓扑，非索引绘制）
  QRhiBuffer* faceNormalBuffer() const { return m_fnormalVbuf; }
  int faceNormalVertexCount() const { return m_faceNormalLines.size(); }

  /// @brief 访问 CPU 端数据（用于拾取、统计等）
  const CpuMesh& cpuMesh() const { return m_cpuData; }

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

 private:
  void buildLineIndices();
  void buildNormalLines();
  void rebuildDerivedData();

  QRhi* m_rhi = nullptr;
  QRhiBuffer* m_vbuf = nullptr;
  QRhiBuffer* m_ibuf = nullptr;
  QRhiBuffer* m_lineIbuf = nullptr;
  QRhiBuffer* m_vnormalVbuf = nullptr;
  QRhiBuffer* m_fnormalVbuf = nullptr;

  CpuMesh m_baseData;   ///< 未变换的原始数据
  CpuMesh m_cpuData;    ///< 当前（已变换）数据
  QVector<quint32> m_lineIndices;
  QVector<LineVertex> m_vertexNormalLines;
  QVector<LineVertex> m_faceNormalLines;

  QString m_name;
  bool m_visible = true;
  QMatrix4x4 m_transform;

  int m_indexCount = 0;
  int m_lineIndexCount = 0;
  bool m_uploaded = false;
};

#endif  // MODELVIEWER_SRC_RENDERER_RHIMESH_H_
