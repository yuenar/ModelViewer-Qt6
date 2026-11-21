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

#include "ModelLoader.h"
#include "../core/CpuMesh.h"

// Assimp 库头文件，用于 3D 模型导入
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

// Qt 头文件
#include <QFileInfo>
#include <QDebug>
#include <QMatrix4x4>
#include <QSet>
#include <functional>

namespace {

/// @brief Assimp 矩阵（行主序）转 Qt 矩阵（构造函数按行主序）
QMatrix4x4 toQtMatrix(const aiMatrix4x4& m) {
  return QMatrix4x4(
      m.a1, m.a2, m.a3, m.a4,
      m.b1, m.b2, m.b3, m.b4,
      m.c1, m.c2, m.c3, m.c4,
      m.d1, m.d2, m.d3, m.d4);
}

/// @brief 读取材质漫反射颜色
QVector3D materialDiffuse(const aiScene* scene, unsigned int materialIndex) {
  if (!scene || materialIndex >= scene->mNumMaterials || !scene->mMaterials) {
    return QVector3D(0.8f, 0.8f, 0.8f);
  }
  aiColor4D color;
  if (aiGetMaterialColor(scene->mMaterials[materialIndex],
                         AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
    return QVector3D(color.r, color.g, color.b);
  }
  return QVector3D(0.8f, 0.8f, 0.8f);
}

/// @brief 将 aiMesh 转成 CpuMesh，并应用节点累积变换
CpuMesh buildMesh(const aiScene* scene, const aiMesh* aiMesh,
                  const QMatrix4x4& transform, const QString& name) {
  CpuMesh cpuMesh;
  cpuMesh.name = name;
  cpuMesh.baseColor = materialDiffuse(scene, aiMesh->mMaterialIndex);

  const QMatrix4x4 normalMatrix = transform.inverted().transposed();

  cpuMesh.vertices.reserve(aiMesh->mNumVertices);
  for (unsigned int j = 0; j < aiMesh->mNumVertices; ++j) {
    CpuMesh::Vertex vertex;
    vertex.position = transform.map(QVector3D(
        aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z));

    if (aiMesh->HasNormals()) {
      vertex.normal = normalMatrix.mapVector(QVector3D(
          aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z)).normalized();
    } else {
      vertex.normal = QVector3D(0, 0, 1);
    }

    if (aiMesh->HasTextureCoords(0)) {
      vertex.texCoord = QVector2D(aiMesh->mTextureCoords[0][j].x,
                                  aiMesh->mTextureCoords[0][j].y);
    }

    cpuMesh.vertices.append(vertex);
    cpuMesh.bbox.expand(vertex.position);
  }

  cpuMesh.indices.reserve(aiMesh->mNumFaces * 3);
  for (unsigned int j = 0; j < aiMesh->mNumFaces; ++j) {
    const aiFace& face = aiMesh->mFaces[j];
    if (face.mNumIndices == 3) {
      for (unsigned int k = 0; k < face.mNumIndices; ++k) {
        cpuMesh.indices.append(face.mIndices[k]);
      }
    }
  }

  return cpuMesh;
}

}  // namespace

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

  // 递归遍历节点树，累积节点变换（修复多节点模型位置错误）
  std::function<void(aiNode*, const QMatrix4x4&)> processNode =
      [&](aiNode* node, const QMatrix4x4& parent) {
        if (!node) {
          return;
        }

        const QMatrix4x4 global = parent * toQtMatrix(node->mTransformation);

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
          const unsigned int meshIndex = node->mMeshes[i];
          if (meshIndex >= scene->mNumMeshes) {
            continue;
          }
          const aiMesh* aiMesh = scene->mMeshes[meshIndex];

          QString meshName = aiMesh->mName.length > 0
              ? QString::fromUtf8(aiMesh->mName.C_Str())
              : QString::fromUtf8(node->mName.C_Str());
          if (meshName.isEmpty()) {
            meshName = QString("Mesh %1").arg(meshes.size());
          }

          CpuMesh cpuMesh = buildMesh(scene, aiMesh, global, meshName);
          if (!cpuMesh.vertices.isEmpty() && !cpuMesh.indices.isEmpty()) {
            meshes.append(cpuMesh);
          }
        }

        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
          processNode(node->mChildren[c], global);
        }
      };

  processNode(scene->mRootNode, QMatrix4x4());

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
