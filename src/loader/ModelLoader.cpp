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

#include "ModelLoader.h"
#include "../core/CpuMesh.h"

// Assimp 库头文件，用于 3D 模型导入
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Qt 头文件
#include <QFileInfo>
#include <QDebug>
#include <QSet>

QVector<CpuMesh> ModelLoader::loadCPU(const QString& filePath) {
  // 提取文件扩展名并转换为小写，用于格式检测
  const QFileInfo info(filePath);
  const QString ext = info.suffix().toLower();
  
  // 检查是否为 Assimp 支持的格式
  if (isAssimpFormat(ext)) {
    return loadWithAssimp(filePath);
  }
  
  // 不支持的文件格式，输出警告信息
  qWarning() << "Unsupported file format:" << ext;
  return {};
}

QVector<CpuMesh> ModelLoader::loadWithAssimp(const QString& path) {
  // 创建 Assimp 导入器实例
  Assimp::Importer importer;
  
  // 设置后处理标志，优化导入的模型数据
  const unsigned int flags = aiProcess_Triangulate |           // 将所有图元转换为三角形
                             aiProcess_GenNormals |              // 为缺失法线的网格生成法线
                             aiProcess_JoinIdenticalVertices |   // 合并重复顶点
                             aiProcess_SortByPType;              // 按图元类型排序
  
  // 读取模型文件，应用指定的后处理
  const aiScene* scene = importer.ReadFile(path.toStdString(), flags);
  
  // 检查导入是否成功
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    qWarning() << "Assimp error:" << importer.GetErrorString();
    return {};
  }
  
  QVector<CpuMesh> meshes;
    
  // 遍历场景中的所有网格
  for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh* aiMesh = scene->mMeshes[i];
    
    CpuMesh cpuMesh;
    
    // 预分配顶点数组内存，提高性能
    cpuMesh.vertices.reserve(aiMesh->mNumVertices);
    
    // 转换顶点数据
    for (unsigned int j = 0; j < aiMesh->mNumVertices; ++j) {
      CpuMesh::Vertex vertex;
      
      // 转换顶点位置坐标
      vertex.position = QVector3D(
          aiMesh->mVertices[j].x,
          aiMesh->mVertices[j].y,
          aiMesh->mVertices[j].z
      );
      
      // 转换顶点法线
      if (aiMesh->HasNormals()) {
        vertex.normal = QVector3D(
            aiMesh->mNormals[j].x,
            aiMesh->mNormals[j].y,
            aiMesh->mNormals[j].z
        );
      } else {
        // 如果没有法线数据，使用默认法线
        vertex.normal = QVector3D(0, 0, 1);
      }
      
      // 转换纹理坐标（只使用第一组纹理坐标）
      if (aiMesh->HasTextureCoords(0)) {
        vertex.texCoord = QVector2D(
            aiMesh->mTextureCoords[0][j].x,
            aiMesh->mTextureCoords[0][j].y
        );
      } else {
        // 如果没有纹理坐标，使用默认值
        vertex.texCoord = QVector2D(0, 0);
      }
      
      // 添加顶点到网格并更新包围盒
      cpuMesh.vertices.append(vertex);
      cpuMesh.bbox.expand(vertex.position);
    }
        
    // 转换索引数据（三角形面片）
    cpuMesh.indices.reserve(aiMesh->mNumFaces * 3);
    for (unsigned int j = 0; j < aiMesh->mNumFaces; ++j) {
      const aiFace& face = aiMesh->mFaces[j];
      // 只处理三角形面片
      if (face.mNumIndices == 3) {
        for (unsigned int k = 0; k < face.mNumIndices; ++k) {
          cpuMesh.indices.append(face.mIndices[k]);
        }
      }
    }
    
    // 只有包含有效顶点和索引的网格才添加到结果中
    if (!cpuMesh.vertices.isEmpty() && !cpuMesh.indices.isEmpty()) {
      meshes.append(cpuMesh);
    }
  }
  
  return meshes;
}

bool ModelLoader::isAssimpFormat(const QString& ext) const {
  // 使用静态集合存储支持的文件格式，避免重复创建
  static const QSet<QString> supportedFormats = {
    // 常用 3D 模型格式
    "obj", "stl", "fbx", "dae", "ply", "x", "3ds", "blend", "ase",
    // 现代格式
    "ifc", "gltf", "glb", "bvh", "csm", "md3", "md5mesh", "md5anim",
    "md5camera", "nff", "nff", "off", "raw", "ac", "ac3d", "kmz",
    // 游戏引擎格式
    "cob", "csm", "bvh", "cmo", "xgl", "zgl", "q3o", "q3s", "ndo",
    "smd", "vta", "mdl", "mdl2", "hmp", "mesh.xml", "ogex", "irrmesh",
    "irr", "q3d", "q3s", "off", "ter", "dxf", "lwo", "lws", "lxo",
    // 其他格式
    "sto", "x", "x3d", "3mf", "mmd", "pmd", "pmx", "vmd", "vpd"
  };
  
  // 检查扩展名是否在支持列表中
  return supportedFormats.contains(ext);
}
