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

#ifndef MODELVIEWER_SRC_LOADER_MODELLOADER_H_
#define MODELVIEWER_SRC_LOADER_MODELLOADER_H_

#include <QString>
#include <QVector>
#include "../core/CpuMesh.h"

/// @brief 3D 模型加载器，负责将各种格式的 3D 模型文件加载为内部数据结构
/// 
/// 该类基于 Assimp 库实现了多格式 3D 模型文件的加载功能。
/// 支持将不同格式的模型文件统一转换为 CpuMesh 数据结构，
/// 供渲染器使用。
/// 
/// 主要功能：
/// - 自动检测文件格式
/// - 加载多种 3D 模型格式（OBJ、FBX、GLTF 等）
/// - 数据格式转换和优化
/// - 错误处理和验证
/// 
/// 使用示例：
/// @code
/// ModelLoader loader;
/// QVector<CpuMesh> meshes = loader.loadCPU("model.obj");
/// 
/// if (!meshes.isEmpty()) {
///     qDebug() << "Loaded" << meshes.size() << "meshes";
///     for (const auto& mesh : meshes) {
///         qDebug() << "Vertices:" << mesh.vertices.size();
///     }
/// }
/// @endcode
class ModelLoader {
 public:
  ModelLoader() = default;
  ~ModelLoader() = default;

  /// @brief 禁用拷贝构造函数
  ModelLoader(const ModelLoader&) = delete;
  
  /// @brief 禁用赋值操作符
  ModelLoader& operator=(const ModelLoader&) = delete;

  /// @brief 加载 3D 模型文件并转换为 CPU 网格数据
  /// 
  /// 该方法会根据文件扩展名自动选择合适的加载方式，
  /// 目前主要使用 Assimp 库进行加载。
  /// 
  /// @param[in] filePath 要加载的模型文件路径
  /// @return QVector<CpuMesh> 加载成功返回网格数据数组，失败返回空数组
  /// 
  /// @note 支持的格式包括：OBJ、FBX、GLTF、3DS、DAE、STL 等
  /// @note 加载失败时会输出错误信息到控制台
  /// 
  /// @warning 文件路径必须存在且可读，否则加载失败
  /// @warning 大型模型文件可能需要较长的加载时间
  /// 
  /// 使用示例：
  /// @code
  /// ModelLoader loader;
  /// QVector<CpuMesh> meshes = loader.loadCPU("car.fbx");
  /// if (meshes.isEmpty()) {
  ///     qWarning() << "Failed to load model";
  /// }
  /// @endcode
  QVector<CpuMesh> loadCPU(const QString& filePath);

 private:
  /// @brief 使用 Assimp 库加载模型文件
  /// 
  /// 该方法是实际执行模型加载的核心函数，通过 Assimp 库
  /// 导入场景数据并将其转换为内部的 CpuMesh 格式。
  /// 
  /// @param[in] path 模型文件的绝对路径
  /// @return QVector<CpuMesh> 转换后的网格数据数组
  /// 
  /// @note 内部处理包括：
  /// - 场景遍历和网格提取
  /// - 坐标系转换（右手坐标系）
  /// - 法线计算和归一化
  /// - 包围盒计算
  /// - 数据优化和清理
  /// 
  /// @warning 该方法假设输入文件格式有效
  QVector<CpuMesh> loadWithAssimp(const QString& path);

  /// @brief 检查文件扩展名是否为 Assimp 支持的格式
  /// 
  /// @param[in] ext 文件扩展名（不包含点号，如 "obj"、"fbx"）
  /// @return bool 如果是支持格式返回 true，否则返回 false
  /// 
  /// @note 检查不区分大小写
  /// @note 支持的格式列表可根据 Assimp 版本变化
  /// 
  /// 使用示例：
  /// @code
  /// bool supported = isAssimpFormat("obj");   // true
  /// bool supported = isAssimpFormat("txt");   // false
  /// @endcode
  bool isAssimpFormat(const QString& ext) const;
};

#endif  // MODELVIEWER_SRC_LOADER_MODELLOADER_H_
