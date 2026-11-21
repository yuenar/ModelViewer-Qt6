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
#include <QDir>
#include <QDebug>
#include <QHash>
#include <QMatrix4x4>
#include <QSet>
#include <QQuaternion>
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

/// @brief 读取材质漫反射贴图（内嵌或外部文件）
QImage loadDiffuseTexture(const aiScene* scene, unsigned int materialIndex,
                          const QString& modelPath) {
  if (!scene || materialIndex >= scene->mNumMaterials || !scene->mMaterials) {
    return QImage();
  }

  aiString texturePath;
  if (aiGetMaterialTexture(scene->mMaterials[materialIndex], aiTextureType_DIFFUSE,
                           0, &texturePath) != AI_SUCCESS) {
    return QImage();
  }

  const aiTexture* texture = scene->GetEmbeddedTexture(texturePath.C_Str());
  if (texture) {
    if (texture->mHeight == 0) {
      return QImage::fromData(reinterpret_cast<const uchar*>(texture->pcData),
                              int(texture->mWidth));
    }
    QImage image(int(texture->mWidth), int(texture->mHeight),
                 QImage::Format_ARGB32);
    for (unsigned int y = 0; y < texture->mHeight; ++y) {
      for (unsigned int x = 0; x < texture->mWidth; ++x) {
        const aiTexel& texel = texture->pcData[y * texture->mWidth + x];
        image.setPixel(int(x), int(y),
                       qRgba(texel.r, texel.g, texel.b, texel.a));
      }
    }
    return image;
  }

  QString path = QString::fromUtf8(texturePath.C_Str());
  path.replace('\\', '/');
  const QString resolved = QFileInfo(modelPath).dir().filePath(path);
  return QImage(resolved);
}

/// @brief 将 aiMesh 转成 CpuMesh，并应用节点累积变换
///
/// @param[in] bakeTransform 为 false 时保留网格局部坐标（骨骼蒙皮网格），
///            包围盒仍按世界空间计算，供相机适配使用。
CpuMesh buildMesh(const aiScene* scene, const aiMesh* aiMesh,
                  const QMatrix4x4& transform, const QString& name,
                  const QImage& diffuseTexture, bool bakeTransform) {
  CpuMesh cpuMesh;
  cpuMesh.name = name;
  cpuMesh.baseColor = materialDiffuse(scene, aiMesh->mMaterialIndex);
  cpuMesh.diffuseTexture = diffuseTexture;

  const QMatrix4x4 normalMatrix = transform.inverted().transposed();

  cpuMesh.vertices.reserve(aiMesh->mNumVertices);
  for (unsigned int j = 0; j < aiMesh->mNumVertices; ++j) {
    CpuMesh::Vertex vertex;
    const QVector3D raw(aiMesh->mVertices[j].x, aiMesh->mVertices[j].y,
                        aiMesh->mVertices[j].z);
    const QVector3D world = transform.map(raw);

    if (bakeTransform) {
      vertex.position = world;
    } else {
      vertex.position = raw;
    }

    if (aiMesh->HasNormals()) {
      const QVector3D normal(aiMesh->mNormals[j].x, aiMesh->mNormals[j].y,
                             aiMesh->mNormals[j].z);
      vertex.normal = bakeTransform
                          ? normalMatrix.mapVector(normal).normalized()
                          : normal.normalized();
    } else {
      vertex.normal = QVector3D(0, 0, 1);
    }

    if (aiMesh->HasTextureCoords(0)) {
      vertex.texCoord = QVector2D(aiMesh->mTextureCoords[0][j].x,
                                  aiMesh->mTextureCoords[0][j].y);
    }

    cpuMesh.vertices.append(vertex);
    cpuMesh.bbox.expand(world);
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

/// @brief 从 aiBone 提取骨骼蒙皮数据
void fillSkinData(
    CpuMesh& mesh, const aiMesh* aiMesh, const QMatrix4x4& meshWorld,
    const QHash<QString, QPair<QVector<QString>, QVector<QMatrix4x4>>>&
        nodeChains) {
  mesh.skinWeights.clear();
  mesh.skinWeights.resize(mesh.vertices.size());

  for (unsigned int b = 0; b < aiMesh->mNumBones; ++b) {
    const aiBone* bone = aiMesh->mBones[b];
    if (!bone) {
      continue;
    }

    const QString boneName = QString::fromUtf8(bone->mName.C_Str());
    const QMatrix4x4 offset = toQtMatrix(bone->mOffsetMatrix);

    QVector<QString> path;
    QVector<QMatrix4x4> restLocals;
    QMatrix4x4 boneWorld = meshWorld;

    auto chainIt = nodeChains.constFind(boneName);
    if (chainIt != nodeChains.constEnd()) {
      path = chainIt.value().first;
      restLocals = chainIt.value().second;
      boneWorld = QMatrix4x4();
      for (const QMatrix4x4& local : restLocals) {
        boneWorld *= local;
      }
    }

    const int boneIndex = mesh.boneNames.size();
    mesh.boneNames.append(boneName);
    mesh.boneOffsets.append(offset);
    mesh.boneBindMatrices.append(boneWorld * offset);
    mesh.bonePaths.append(path);
    mesh.boneRestLocals.append(restLocals);

    for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
      const aiVertexWeight& weight = bone->mWeights[w];
      if (weight.mVertexId >= mesh.skinWeights.size() ||
          weight.mWeight <= 1e-6f) {
        continue;
      }

      CpuMesh::SkinVertex& skin = mesh.skinWeights[weight.mVertexId];
      int slot = -1;
      for (int k = 0; k < 4; ++k) {
        if (skin.weight[k] <= 1e-6f) {
          slot = k;
          break;
        }
      }
      if (slot < 0) {
        slot = 0;
        for (int k = 1; k < 4; ++k) {
          if (skin.weight[k] < skin.weight[slot]) {
            slot = k;
          }
        }
      }
      skin.boneIndex[slot] = quint32(boneIndex);
      skin.weight[slot] = weight.mWeight;
    }
  }

  // 归一化权重；无权重顶点回退到第一根骨骼
  for (CpuMesh::SkinVertex& skin : mesh.skinWeights) {
    float sum = 0.0f;
    for (int k = 0; k < 4; ++k) {
      sum += skin.weight[k];
    }
    if (sum <= 1e-6f) {
      skin.boneIndex[0] = 0;
      skin.weight[0] = 1.0f;
    } else {
      for (int k = 0; k < 4; ++k) {
        skin.weight[k] /= sum;
      }
    }
  }
}

}  // namespace

ModelAsset ModelLoader::loadCPU(const QString& filePath) {
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

ModelAsset ModelLoader::loadWithAssimp(const QString& path) {
  ModelAsset asset;

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
    return asset;
  }
  
  QVector<CpuMesh>& meshes = asset.meshes;
  QHash<unsigned int, QImage> textureCache;
  QHash<QString, aiNode*> nodeMap;
  // 节点名 -> (根到该节点的名称链, 静止局部变换)
  QHash<QString, QPair<QVector<QString>, QVector<QMatrix4x4>>> nodeChains;

  // 蒙皮数据需要等整棵节点树遍历完（骨骼节点可能在网格节点之后）再填充
  struct PendingSkin {
    int meshIndex = 0;
    const aiMesh* mesh = nullptr;
    QMatrix4x4 world;
  };
  QVector<PendingSkin> pendingSkins;

  // 当前遍历路径（根 → 当前节点）及其静止局部变换
  QVector<QString> nodePath;
  QVector<QMatrix4x4> nodeLocals;

  // 递归遍历节点树，累积节点变换（修复多节点模型位置错误）
  std::function<void(aiNode*, const QMatrix4x4&)> processNode =
      [&](aiNode* node, const QMatrix4x4& parent) {
        if (!node) {
          return;
        }

        const QMatrix4x4 local = toQtMatrix(node->mTransformation);
        const QMatrix4x4 global = parent * local;

        QString nodeName = QString::fromUtf8(node->mName.C_Str());
        if (nodeName.isEmpty()) {
          nodeName = QString("Node %1").arg(nodeMap.size());
        }
        nodeMap.insert(nodeName, node);
        nodePath.append(nodeName);
        nodeLocals.append(local);
        nodeChains.insert(nodeName, qMakePair(nodePath, nodeLocals));

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
          const unsigned int meshIndex = node->mMeshes[i];
          if (meshIndex >= scene->mNumMeshes) {
            continue;
          }
          const aiMesh* aiMesh = scene->mMeshes[meshIndex];

          QString meshName = aiMesh->mName.length > 0
              ? QString::fromUtf8(aiMesh->mName.C_Str())
              : nodeName;
          if (meshName.isEmpty()) {
            meshName = QString("Mesh %1").arg(meshes.size());
          }

          const unsigned int materialIndex = aiMesh->mMaterialIndex;
          auto it = textureCache.find(materialIndex);
          if (it == textureCache.end()) {
            it = textureCache.insert(
                materialIndex,
                loadDiffuseTexture(scene, materialIndex, path));
          }

          CpuMesh cpuMesh = buildMesh(scene, aiMesh, global, meshName,
                                      it.value(), aiMesh->mNumBones == 0);
          if (!cpuMesh.vertices.isEmpty() && !cpuMesh.indices.isEmpty()) {
            cpuMesh.nodeName = nodeName;
            cpuMesh.nodePath = nodePath;
            cpuMesh.nodeRestLocals = nodeLocals;
            cpuMesh.restWorld = global;
            if (aiMesh->mNumBones > 0) {
              pendingSkins.append({int(meshes.size()), aiMesh, global});
            }
            meshes.append(cpuMesh);
          }
        }

        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
          processNode(node->mChildren[c], global);
        }

        nodePath.removeLast();
        nodeLocals.removeLast();
      };

  processNode(scene->mRootNode, QMatrix4x4());

  for (const PendingSkin& pending : pendingSkins) {
    if (pending.meshIndex >= 0 && pending.meshIndex < meshes.size() &&
        pending.mesh) {
      fillSkinData(meshes[pending.meshIndex], pending.mesh, pending.world,
                   nodeChains);
    }
  }

  // ── 提取节点关键帧动画 ───────────────────────────────────
  const double defaultTicksPerSecond = 25.0;
  for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai) {
    const aiAnimation* anim = scene->mAnimations[ai];
    if (!anim) {
      continue;
    }

    const double ticksPerSecond =
        anim->mTicksPerSecond > 1e-6 ? anim->mTicksPerSecond
                                     : defaultTicksPerSecond;

    AnimationClip clip;
    clip.name = anim->mName.length > 0
                    ? QString::fromUtf8(anim->mName.C_Str())
                    : QString("Animation %1").arg(ai);
    clip.duration = float(anim->mDuration / ticksPerSecond);

    float maxChannelTime = 0.0f;

    for (unsigned int ci = 0; ci < anim->mNumChannels; ++ci) {
      const aiNodeAnim* nodeAnim = anim->mChannels[ci];
      if (!nodeAnim) {
        continue;
      }

      AnimationChannel channel;
      channel.nodeName = QString::fromUtf8(nodeAnim->mNodeName.C_Str());

      // 静止姿态：用于补齐缺失的关键帧分量
      auto nodeIt = nodeMap.find(channel.nodeName);
      if (nodeIt != nodeMap.end() && nodeIt.value()) {
        aiVector3D scale;
        aiQuaternion rotation;
        aiVector3D position;
        nodeIt.value()->mTransformation.Decompose(scale, rotation, position);
        channel.restPosition = QVector3D(position.x, position.y, position.z);
        channel.restRotation =
            QQuaternion(rotation.w, rotation.x, rotation.y, rotation.z);
        channel.restScale = QVector3D(scale.x, scale.y, scale.z);
      }

      channel.positionTimes.reserve(int(nodeAnim->mNumPositionKeys));
      channel.positionValues.reserve(int(nodeAnim->mNumPositionKeys));
      for (unsigned int k = 0; k < nodeAnim->mNumPositionKeys; ++k) {
        const aiVectorKey& key = nodeAnim->mPositionKeys[k];
        const float time = float(key.mTime / ticksPerSecond);
        channel.positionTimes.append(time);
        channel.positionValues.append(
            QVector3D(key.mValue.x, key.mValue.y, key.mValue.z));
        maxChannelTime = qMax(maxChannelTime, time);
      }

      channel.rotationTimes.reserve(int(nodeAnim->mNumRotationKeys));
      channel.rotationValues.reserve(int(nodeAnim->mNumRotationKeys));
      for (unsigned int k = 0; k < nodeAnim->mNumRotationKeys; ++k) {
        const aiQuatKey& key = nodeAnim->mRotationKeys[k];
        const float time = float(key.mTime / ticksPerSecond);
        channel.rotationTimes.append(time);
        channel.rotationValues.append(
            QQuaternion(key.mValue.w, key.mValue.x, key.mValue.y,
                        key.mValue.z));
        maxChannelTime = qMax(maxChannelTime, time);
      }

      channel.scaleTimes.reserve(int(nodeAnim->mNumScalingKeys));
      channel.scaleValues.reserve(int(nodeAnim->mNumScalingKeys));
      for (unsigned int k = 0; k < nodeAnim->mNumScalingKeys; ++k) {
        const aiVectorKey& key = nodeAnim->mScalingKeys[k];
        const float time = float(key.mTime / ticksPerSecond);
        channel.scaleTimes.append(time);
        channel.scaleValues.append(
            QVector3D(key.mValue.x, key.mValue.y, key.mValue.z));
        maxChannelTime = qMax(maxChannelTime, time);
      }

      if (!channel.positionTimes.isEmpty() ||
          !channel.rotationTimes.isEmpty() ||
          !channel.scaleTimes.isEmpty()) {
        clip.channels.append(channel);
      }
    }

    clip.duration = qMax(clip.duration, maxChannelTime);
    if (!clip.channels.isEmpty() && clip.duration > 0.0f) {
      asset.animations.append(clip);
    }
  }

  return asset;
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
