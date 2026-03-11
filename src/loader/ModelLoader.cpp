#include "ModelLoader.h"
#include "../core/CpuMesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <QFileInfo>
#include <QDebug>
#include <QSet>

QVector<CpuMesh> ModelLoader::loadCPU(const QString& filePath) {
    const QFileInfo info(filePath);
    const QString ext = info.suffix().toLower();
    
    if (isAssimpFormat(ext)) {
        return loadWithAssimp(filePath);
    }
    
    qWarning() << "Unsupported file format:" << ext;
    return {};
}

QVector<CpuMesh> ModelLoader::loadWithAssimp(const QString& path) {
    Assimp::Importer importer;
    
    const unsigned int flags = aiProcess_Triangulate |
                               aiProcess_GenNormals |
                               aiProcess_JoinIdenticalVertices |
                               aiProcess_SortByPType;
    
    const aiScene* scene = importer.ReadFile(path.toStdString(), flags);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        qWarning() << "Assimp error:" << importer.GetErrorString();
        return {};
    }
    
    QVector<CpuMesh> meshes;
    
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* aiMesh = scene->mMeshes[i];
        
        CpuMesh cpuMesh;
        
        // 转换顶点
        cpuMesh.vertices.reserve(aiMesh->mNumVertices);
        for (unsigned int j = 0; j < aiMesh->mNumVertices; ++j) {
            CpuMesh::Vertex vertex;
            
            vertex.position = QVector3D(
                aiMesh->mVertices[j].x,
                aiMesh->mVertices[j].y,
                aiMesh->mVertices[j].z
            );
            
            if (aiMesh->HasNormals()) {
                vertex.normal = QVector3D(
                    aiMesh->mNormals[j].x,
                    aiMesh->mNormals[j].y,
                    aiMesh->mNormals[j].z
                );
            } else {
                vertex.normal = QVector3D(0, 0, 1);
            }
            
            if (aiMesh->HasTextureCoords(0)) {
                vertex.texCoord = QVector2D(
                    aiMesh->mTextureCoords[0][j].x,
                    aiMesh->mTextureCoords[0][j].y
                );
            } else {
                vertex.texCoord = QVector2D(0, 0);
            }
            
            cpuMesh.vertices.append(vertex);
            cpuMesh.bbox.expand(vertex.position);
        }
        
        // 转换索引
        cpuMesh.indices.reserve(aiMesh->mNumFaces * 3);
        for (unsigned int j = 0; j < aiMesh->mNumFaces; ++j) {
            const aiFace& face = aiMesh->mFaces[j];
            if (face.mNumIndices == 3) {
                for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                    cpuMesh.indices.append(face.mIndices[k]);
                }
            }
        }
        
        if (!cpuMesh.vertices.isEmpty() && !cpuMesh.indices.isEmpty()) {
            meshes.append(cpuMesh);
        }
    }
    
    return meshes;
}

bool ModelLoader::isAssimpFormat(const QString& ext) const {
    static const QSet<QString> supportedFormats = {
        "obj", "stl", "fbx", "dae", "ply", "x", "3ds", "blend", "ase",
        "ifc", "gltf", "glb", "bvh", "csm", "md3", "md5mesh", "md5anim",
        "md5camera", "nff", "nff", "off", "raw", "ac", "ac3d", "kmz",
        "cob", "csm", "bvh", "cmo", "xgl", "zgl", "q3o", "q3s", "ndo",
        "smd", "vta", "mdl", "mdl2", "hmp", "mesh.xml", "ogex", "irrmesh",
        "irr", "q3d", "q3s", "off", "ter", "dxf", "lwo", "lws", "lxo",
        "sto", "x", "x3d", "3mf", "mmd", "pmd", "pmx", "vmd", "vpd"
    };
    
    return supportedFormats.contains(ext);
}
