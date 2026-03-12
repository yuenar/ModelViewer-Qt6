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
#include "../core/CpuMesh.h"

/// @brief GPU 端网格数据管理类
/// 
/// 该类负责管理 GPU 上的网格数据，包括顶点缓冲区和索引缓冲区。
/// 将 CPU 端的 CpuMesh 数据上传到 GPU，并提供渲染所需的缓冲区访问接口。
/// 
/// 主要功能：
/// - 创建和管理 GPU 顶点缓冲区（VBO）
/// - 创建和管理 GPU 索引缓冲区（IBO/EBO）
/// - 数据上传和同步
/// - 资源生命周期管理
/// 
/// 使用流程：
/// 1. 从 CpuMesh 创建 RhiMesh
/// 2. 调用 upload() 将数据上传到 GPU
/// 3. 在渲染时获取缓冲区进行绘制
/// 
/// 使用示例：
/// @code
/// RhiMesh gpuMesh(rhi, cpuMesh);
/// gpuMesh.upload(&resourceBatch);
/// 
/// // 渲染时使用
/// QRhiBuffer* vbuf = gpuMesh.vertexBuffer();
/// QRhiBuffer* ibuf = gpuMesh.indexBuffer();
/// int indexCount = gpuMesh.indexCount();
/// @endcode
class RhiMesh {
 public:
  /// @brief 构造函数，从 CPU 网格数据创建 GPU 网格
  /// 
  /// 根据提供的 CpuMesh 数据创建相应的 GPU 缓冲区。
  /// 构造函数会创建缓冲区对象，但不会立即上传数据。
  /// 
  /// @param[in] rhi RHI 设备指针，用于创建 GPU 资源
  /// @param[in] cpu CPU 端的网格数据，包含顶点和索引信息
  /// 
  /// @note 数据上传需要显式调用 upload() 方法
  /// @note rhi 指针必须在 RhiMesh 生命周期内有效
  RhiMesh(QRhi* rhi, const CpuMesh& cpu);
  
  /// @brief 析构函数，清理 GPU 资源
  /// 
  /// 自动释放创建的顶点缓冲区和索引缓冲区。
  /// 如果缓冲区仍被 GPU 使用，会安全地延迟释放。
  ~RhiMesh();
  
  /// @brief 设置 RHI 设备指针
  /// 
  /// 在 RHI 设备重新创建或更改时使用。
  /// 
  /// @param[in] rhi 新的 RHI 设备指针
  /// 
  /// @note 更换 RHI 后需要重新调用 upload() 上传数据
  void setRhi(QRhi* rhi) { m_rhi = rhi; }
  
  /// @brief 将 CPU 数据上传到 GPU
  /// 
  /// 将 CpuMesh 中的顶点和索引数据上传到 GPU 缓冲区。
  /// 使用资源更新批次进行高效的批量上传。
  /// 
  /// @param[in] batch 资源更新批次，用于批量提交更新操作
  /// 
  /// @note 必须在渲染线程中调用
  /// @note 每次数据修改后都需要重新上传
  void upload(QRhiResourceUpdateBatch* batch);
  
  /// @brief 获取顶点缓冲区
  /// 
  /// @return QRhiPtr* GPU 顶点缓冲区指针
  /// 
  /// @note 返回的缓冲区包含位置、法线、纹理坐标等顶点属性
  QRhiBuffer* vertexBuffer() const { return m_vbuf; }
  
  /// @brief 获取索引缓冲区
  /// 
  /// @return QRhiPtr* GPU 索引缓冲区指针
  /// 
  /// @note 索引类型通常为 32 位无符号整数
  QRhiBuffer* indexBuffer() const { return m_ibuf; }
  
  /// @brief 获取索引数量
  /// 
  /// @return int 索引缓冲区中的索引数量
  /// 
  /// @note 用于绘制调用：glDrawElements(GL_TRIANGLES, count, ...)
  int indexCount() const { return m_indexCount; }
  
  /// @brief 获取顶点数量
  /// 
  /// @return int 顶点缓冲区中的顶点数量
  int vertexCount() const { return m_cpuData.vertices.size(); }

 private:
  /// @brief RHI 设备指针
  /// 
  /// 用于创建和管理 GPU 资源。
  /// 所有 GPU 操作都通过此接口进行。
  QRhi* m_rhi = nullptr;
  
  /// @brief GPU 顶点缓冲区
  /// 
  /// 存储网格的顶点数据，包括：
  /// - 位置坐标（3 个 float）
  /// - 法线向量（3 个 float）
  /// - 纹理坐标（2 个 float）
  /// 
  /// 缓冲区格式由顶点布局定义。
  QRhiBuffer* m_vbuf = nullptr;
  
  /// @brief GPU 索引缓冲区
  /// 
  /// 存储网格的索引数据，定义三角形的拓扑结构。
  /// 每三个索引构成一个三角形。
  QRhiBuffer* m_ibuf = nullptr;
  
  /// @brief CPU 端网格数据备份
  /// 
  /// 保存原始的 CPU 数据，用于重新上传或数据修改。
  /// 在需要重新创建 GPU 资源时会用到。
  CpuMesh m_cpuData;
  
  /// @brief 索引数量
  /// 
  /// 缓存索引缓冲区中的索引数量，
  /// 避免每次都从缓冲区获取。
  int m_indexCount = 0;
};

#endif  // MODELVIEWER_SRC_RENDERER_RHIMESH_H_
