#include "RhiMesh.h"

#include "BufferPool.h"

#include <QHash>
#include <QSet>
#include <QtMath>

namespace {

quint32 roundBufferSize(quint32 size) {
    const quint32 granularity = 4096;
    return ((size + granularity - 1) / granularity) * granularity;
}

}  // namespace

RhiMesh::RhiMesh(QRhi* rhi, const CpuMesh& cpu)
    : m_rhi(rhi), m_baseData(cpu), m_cpuData(cpu),
      m_indexCount(cpu.indices.size())
{
    m_material.diffuse = cpu.baseColor;
    initSkinData(cpu);
    if (m_skinned) {
        updateBoneMatrices(nullptr);
    }
    rebuildCpuData();
}

RhiMesh::~RhiMesh() {
    releaseGpu();
}

void RhiMesh::setTransform(const QMatrix4x4& transform) {
    m_userTransform = transform;
    m_transform = m_animDelta * m_userTransform;
    rebuildCpuData();
    // GPU 蒙皮时顶点缓冲保存局部坐标，用户变换由 SkinUBO 提供
    if (!m_gpuSkin) {
        releaseGpu();
    }
}

void RhiMesh::setAnimationDelta(const QMatrix4x4& delta) {
    // 忽略近乎相同的变换，避免动画播放时无谓重建
    bool same = true;
    const float* a = delta.constData();
    const float* b = m_animDelta.constData();
    for (int i = 0; i < 16; ++i) {
        if (qAbs(a[i] - b[i]) > 1e-6f) {
            same = false;
            break;
        }
    }
    if (same) {
        return;
    }

    m_animDelta = delta;
    m_transform = m_animDelta * m_userTransform;
    rebuildCpuData();
    if (!m_gpuSkin) {
        releaseGpu();
    }
}

RhiMesh* RhiMesh::clone() const {
    auto* copy = new RhiMesh(m_rhi, m_baseData);
    copy->m_pool = m_pool;
    copy->m_name = m_name;
    copy->m_visible = m_visible;
    copy->m_material = m_material;
    copy->m_animDelta = m_animDelta;
    copy->m_userTransform = m_userTransform;
    copy->m_transform = m_animDelta * m_userTransform;
    copy->m_boneMatrices = m_boneMatrices;
    copy->m_boneNormalMatrices = m_boneNormalMatrices;
    copy->rebuildCpuData();
    return copy;
}

void RhiMesh::initSkinData(const CpuMesh& cpu) {
    m_skinned = cpu.isSkinned();
    if (!m_skinned) {
        return;
    }

    m_skinWeights = cpu.skinWeights;
    m_boneNames = cpu.boneNames;
    m_boneOffsets = cpu.boneOffsets;
    m_boneBindMatrices = cpu.boneBindMatrices;
    m_bonePaths = cpu.bonePaths;
    m_boneRestLocals = cpu.boneRestLocals;
    m_boneMatrices = m_boneBindMatrices;
    m_gpuSkin = !m_boneNames.isEmpty() &&
                m_boneNames.size() <= kMaxGpuBones &&
                m_boneOffsets.size() == m_boneNames.size();
    m_boneNormalMatrices.resize(m_boneMatrices.size());
    for (int i = 0; i < m_boneMatrices.size(); ++i) {
        m_boneNormalMatrices[i] = m_boneMatrices[i].inverted().transposed();
    }
}

void RhiMesh::updateBoneMatrices(const QHash<QString, QMatrix4x4>* poses) {
    const int count = m_boneNames.size();
    m_boneMatrices.resize(count);
    m_boneNormalMatrices.resize(count);

    for (int i = 0; i < count; ++i) {
        QMatrix4x4 matrix;

        if (i < m_bonePaths.size() && !m_bonePaths[i].isEmpty()) {
            const QVector<QString>& path = m_bonePaths[i];
            const QVector<QMatrix4x4>& rest =
                (i < m_boneRestLocals.size()) ? m_boneRestLocals[i]
                                              : QVector<QMatrix4x4>();
            QMatrix4x4 world;
            for (int j = 0; j < path.size(); ++j) {
                QMatrix4x4 local =
                    (j < rest.size()) ? rest[j] : QMatrix4x4();
                if (poses) {
                    auto it = poses->constFind(path[j]);
                    if (it != poses->constEnd()) {
                        local = it.value();
                    }
                }
                world *= local;
            }
            const QMatrix4x4 offset =
                (i < m_boneOffsets.size()) ? m_boneOffsets[i] : QMatrix4x4();
            matrix = world * offset;
        } else if (i < m_boneBindMatrices.size()) {
            matrix = m_boneBindMatrices[i];
        }

        m_boneMatrices[i] = matrix;
        m_boneNormalMatrices[i] = matrix.inverted().transposed();
    }
}

void RhiMesh::skinVertices() {
    const int vertexCount = qMin(m_cpuData.vertices.size(),
                                 m_skinWeights.size());
    for (int i = 0; i < vertexCount; ++i) {
        const CpuMesh::Vertex& src = m_baseData.vertices[i];
        const CpuMesh::SkinVertex& skin = m_skinWeights[i];

        QVector3D position;
        QVector3D normal;
        for (int k = 0; k < 4; ++k) {
            const float weight = skin.weight[k];
            if (weight <= 1e-6f) {
                continue;
            }
            const int bone = int(skin.boneIndex[k]);
            if (bone < 0 || bone >= m_boneMatrices.size()) {
                continue;
            }
            position += weight * m_boneMatrices[bone].map(src.position);
            normal += weight * m_boneNormalMatrices[bone].mapVector(src.normal);
        }

        m_cpuData.vertices[i].position = position;
        if (normal.lengthSquared() > 1e-12f) {
            m_cpuData.vertices[i].normal = normal.normalized();
        } else {
            m_cpuData.vertices[i].normal = src.normal;
        }
    }
}

void RhiMesh::applyBonePoses(const QHash<QString, QMatrix4x4>& poses) {
    if (!m_skinned) {
        return;
    }
    updateBoneMatrices(&poses);
    if (!m_gpuSkin) {
        rebuildCpuData(false);
        releaseGpu();
    }
}

void RhiMesh::resetSkinPose() {
    if (!m_skinned) {
        return;
    }
    updateBoneMatrices(nullptr);
    if (!m_gpuSkin) {
        rebuildCpuData(false);
        releaseGpu();
    }
}

void RhiMesh::syncCpuSkinFromBones() {
    if (!m_skinned) {
        return;
    }
    // 只更新 CPU 侧数据，不触碰 GPU 缓冲
    rebuildCpuData(false);
}

void RhiMesh::rebuildCpuData(bool updateDerived) {
    m_cpuData.vertices = m_baseData.vertices;

    if (m_skinned) {
        skinVertices();
    }

    const QMatrix4x4 normalMatrix = m_transform.inverted().transposed();
    for (auto& vertex : m_cpuData.vertices) {
        vertex.position = m_transform.map(vertex.position);
        vertex.normal = normalMatrix.mapVector(vertex.normal).normalized();
    }

    m_cpuData.bbox = BoundingBox();
    for (const auto& vertex : m_cpuData.vertices) {
        m_cpuData.bbox.expand(vertex.position);
    }

    m_center = m_cpuData.bbox.isEmpty() ? QVector3D() : m_cpuData.bbox.center();
    m_radius = m_cpuData.bbox.isEmpty()
                   ? 1.0f
                   : (m_cpuData.bbox.max() - m_center).length();

    m_indexCount = m_cpuData.indices.size();
    if (updateDerived) {
        rebuildDerivedData();
    } else {
        // 蒙皮每帧更新：拓扑与 LOD 不变，只需刷新法线线段
        buildNormalLines();
    }
}

void RhiMesh::rebuildDerivedData() {
    buildLineIndices();
    buildNormalLines();
    buildLodIndices(1, 12);
    buildLodIndices(2, 6);
}

void RhiMesh::buildLineIndices() {
    m_lineIndices.clear();
    m_lineIndexCount = 0;

    const int triangleCount = m_cpuData.indices.size() / 3;
    if (triangleCount <= 0) {
        return;
    }

    // 每条边只保留一次：key = (min << 32) | max
    QSet<quint64> seen;
    seen.reserve(triangleCount * 3);

    auto addEdge = [this, &seen](quint32 a, quint32 b) {
        if (a == b) {
            return;
        }
        const quint32 lo = qMin(a, b);
        const quint32 hi = qMax(a, b);
        const quint64 key = (static_cast<quint64>(lo) << 32) | hi;
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key);
        m_lineIndices.append(a);
        m_lineIndices.append(b);
    };

    for (int i = 0; i < triangleCount; ++i) {
        const quint32 i0 = m_cpuData.indices[i * 3 + 0];
        const quint32 i1 = m_cpuData.indices[i * 3 + 1];
        const quint32 i2 = m_cpuData.indices[i * 3 + 2];
        addEdge(i0, i1);
        addEdge(i1, i2);
        addEdge(i2, i0);
    }

    m_lineIndexCount = m_lineIndices.size();
}

void RhiMesh::buildNormalLines() {
    m_vertexNormalLines.clear();
    m_faceNormalLines.clear();

    // 顶点法线：每个顶点生成一条线段（起点 + 方向）
    m_vertexNormalLines.reserve(m_cpuData.vertices.size() * 2);
    for (const auto& vertex : m_cpuData.vertices) {
        m_vertexNormalLines.append({vertex.position, QVector3D(0, 0, 0)});
        m_vertexNormalLines.append({vertex.position, vertex.normal.normalized()});
    }

    // 面法线：每个三角形生成一条从面心出发的线段
    const int triangleCount = m_cpuData.indices.size() / 3;
    m_faceNormalLines.reserve(triangleCount * 2);
    for (int i = 0; i < triangleCount; ++i) {
        const quint32 i0 = m_cpuData.indices[i * 3 + 0];
        const quint32 i1 = m_cpuData.indices[i * 3 + 1];
        const quint32 i2 = m_cpuData.indices[i * 3 + 2];
        if (i0 >= static_cast<quint32>(m_cpuData.vertices.size()) ||
            i1 >= static_cast<quint32>(m_cpuData.vertices.size()) ||
            i2 >= static_cast<quint32>(m_cpuData.vertices.size())) {
            continue;
        }

        const QVector3D& p0 = m_cpuData.vertices[i0].position;
        const QVector3D& p1 = m_cpuData.vertices[i1].position;
        const QVector3D& p2 = m_cpuData.vertices[i2].position;

        const QVector3D normal = QVector3D::crossProduct(p1 - p0, p2 - p0);
        if (normal.lengthSquared() < 1e-12f) {
            continue;
        }

        const QVector3D centroid = (p0 + p1 + p2) / 3.0f;
        m_faceNormalLines.append({centroid, QVector3D(0, 0, 0)});
        m_faceNormalLines.append({centroid, normal.normalized()});
    }
}

void RhiMesh::buildLodIndices(int level, int cells) {
    const int index = level - 1;
    if (index < 0 || index >= kLodLevelCount) {
        return;
    }

    m_lodIndices[index].clear();
    m_lodIndexCount[index] = 0;

    const int triangleCount = m_cpuData.indices.size() / 3;
    if (triangleCount < 64 || m_cpuData.vertices.isEmpty() || cells < 2) {
        return;
    }

    const QVector3D min = m_cpuData.bbox.min();
    const QVector3D max = m_cpuData.bbox.max();
    QVector3D extent = max - min;
    extent.setX(qMax(extent.x(), 1e-6f));
    extent.setY(qMax(extent.y(), 1e-6f));
    extent.setZ(qMax(extent.z(), 1e-6f));

    // 顶点聚类：每个网格单元只保留一个代表顶点
    QHash<quint64, quint32> cellToVertex;
    cellToVertex.reserve(m_cpuData.vertices.size());
    QVector<quint32> remap(m_cpuData.vertices.size());

    for (int i = 0; i < m_cpuData.vertices.size(); ++i) {
        const QVector3D& p = m_cpuData.vertices[i].position;
        const int gx = qBound(0, int((p.x() - min.x()) / extent.x() * cells), cells - 1);
        const int gy = qBound(0, int((p.y() - min.y()) / extent.y() * cells), cells - 1);
        const int gz = qBound(0, int((p.z() - min.z()) / extent.z() * cells), cells - 1);
        const quint64 key = (quint64(gx) * cells + gy) * cells + gz;

        auto it = cellToVertex.find(key);
        if (it != cellToVertex.end()) {
            remap[i] = it.value();
        } else {
            cellToVertex.insert(key, quint32(i));
            remap[i] = quint32(i);
        }
    }

    for (int i = 0; i < triangleCount; ++i) {
        const quint32 a = remap[m_cpuData.indices[i * 3 + 0]];
        const quint32 b = remap[m_cpuData.indices[i * 3 + 1]];
        const quint32 c = remap[m_cpuData.indices[i * 3 + 2]];
        if (a == b || b == c || a == c) {
            continue;
        }
        m_lodIndices[index].append(a);
        m_lodIndices[index].append(b);
        m_lodIndices[index].append(c);
    }

    // 简化收益不明显时放弃该级别
    if (m_lodIndices[index].size() > m_cpuData.indices.size() * 7 / 10) {
        m_lodIndices[index].clear();
    }
    m_lodIndexCount[index] = m_lodIndices[index].size();
}

QRhiBuffer* RhiMesh::createBuffer(QRhiResourceUpdateBatch* batch,
                                  QRhiBuffer::UsageFlag usage, const void* data,
                                  quint32 size, quint32* capacity) {
    if (!m_rhi || size == 0) {
        *capacity = 0;
        return nullptr;
    }

    if (m_pool) {
        QRhiBuffer* buffer = m_pool->acquire(usage, size);
        if (buffer) {
            batch->updateDynamicBuffer(buffer, 0, size, data);
            *capacity = roundBufferSize(size);
            return buffer;
        }
    }

    QRhiBuffer* buffer = m_rhi->newBuffer(QRhiBuffer::Immutable, usage, size);
    if (!buffer->create()) {
        delete buffer;
        *capacity = 0;
        return nullptr;
    }
    batch->uploadStaticBuffer(buffer, data);
    *capacity = size;
    return buffer;
}

void RhiMesh::destroyBuffer(QRhiBuffer** buffer, QRhiBuffer::UsageFlag usage,
                            quint32 capacity) {
    if (!*buffer) {
        return;
    }
    if (m_pool) {
        m_pool->release(*buffer, usage, capacity);
    } else {
        delete *buffer;
    }
    *buffer = nullptr;
}

void RhiMesh::upload(QRhiResourceUpdateBatch* batch) {
    if (m_uploaded || !m_rhi) {
        return;
    }

    // GPU 蒙皮网格上传局部坐标；CPU 蒙皮/普通网格上传已变换坐标
    const QVector<CpuMesh::Vertex>& gpuVertices =
        m_gpuSkin ? m_baseData.vertices : m_cpuData.vertices;
    const quint32 vbufSize = gpuVertices.size() * sizeof(CpuMesh::Vertex);
    const quint32 ibufSize = m_cpuData.indices.size() * sizeof(quint32);

    if (vbufSize > 0 && !m_vbuf) {
        m_vbuf = createBuffer(batch, QRhiBuffer::VertexBuffer,
                              gpuVertices.constData(), vbufSize,
                              &m_vbufCapacity);
    }

    if (m_gpuSkin && !m_skinVBuf) {
        QVector<SkinGpuVertex> skinData;
        skinData.reserve(m_skinWeights.size());
        for (const CpuMesh::SkinVertex& skin : m_skinWeights) {
            SkinGpuVertex vertex;
            for (int k = 0; k < 4; ++k) {
                vertex.boneIndex[k] = skin.boneIndex[k];
                vertex.weight[k] = skin.weight[k];
            }
            skinData.append(vertex);
        }
        if (!skinData.isEmpty()) {
            m_skinVBuf = createBuffer(
                batch, QRhiBuffer::VertexBuffer, skinData.constData(),
                quint32(skinData.size() * sizeof(SkinGpuVertex)),
                &m_skinVCapacity);
        }
    }

    if (ibufSize > 0 && !m_ibuf) {
        m_ibuf = createBuffer(batch, QRhiBuffer::IndexBuffer,
                              m_cpuData.indices.constData(), ibufSize,
                              &m_ibufCapacity);
    }

    const quint32 lineIbufSize = m_lineIndices.size() * sizeof(quint32);
    if (lineIbufSize > 0 && !m_lineIbuf) {
        m_lineIbuf = createBuffer(batch, QRhiBuffer::IndexBuffer,
                                  m_lineIndices.constData(), lineIbufSize,
                                  &m_lineIbufCapacity);
    }

    const quint32 vnormalSize = m_vertexNormalLines.size() * sizeof(LineVertex);
    if (vnormalSize > 0 && !m_vnormalVbuf) {
        m_vnormalVbuf = createBuffer(batch, QRhiBuffer::VertexBuffer,
                                     m_vertexNormalLines.constData(),
                                     vnormalSize, &m_vnormalCapacity);
    }

    const quint32 fnormalSize = m_faceNormalLines.size() * sizeof(LineVertex);
    if (fnormalSize > 0 && !m_fnormalVbuf) {
        m_fnormalVbuf = createBuffer(batch, QRhiBuffer::VertexBuffer,
                                     m_faceNormalLines.constData(),
                                     fnormalSize, &m_fnormalCapacity);
    }

    for (int level = 0; level < kLodLevelCount; ++level) {
        const quint32 lodSize = m_lodIndices[level].size() * sizeof(quint32);
        if (lodSize > 0 && !m_lodIbuf[level]) {
            m_lodIbuf[level] = createBuffer(batch, QRhiBuffer::IndexBuffer,
                                            m_lodIndices[level].constData(),
                                            lodSize, &m_lodIbufCapacity[level]);
        }
    }

    m_uploaded = true;
}

void RhiMesh::releaseGpu() {
    destroyBuffer(&m_vbuf, QRhiBuffer::VertexBuffer, m_vbufCapacity);
    destroyBuffer(&m_ibuf, QRhiBuffer::IndexBuffer, m_ibufCapacity);
    destroyBuffer(&m_lineIbuf, QRhiBuffer::IndexBuffer, m_lineIbufCapacity);
    destroyBuffer(&m_vnormalVbuf, QRhiBuffer::VertexBuffer, m_vnormalCapacity);
    destroyBuffer(&m_fnormalVbuf, QRhiBuffer::VertexBuffer, m_fnormalCapacity);
    destroyBuffer(&m_skinVBuf, QRhiBuffer::VertexBuffer, m_skinVCapacity);
    for (int level = 0; level < kLodLevelCount; ++level) {
        destroyBuffer(&m_lodIbuf[level], QRhiBuffer::IndexBuffer,
                      m_lodIbufCapacity[level]);
        m_lodIbufCapacity[level] = 0;
    }

    m_vbufCapacity = 0;
    m_ibufCapacity = 0;
    m_lineIbufCapacity = 0;
    m_vnormalCapacity = 0;
    m_fnormalCapacity = 0;
    m_skinVCapacity = 0;
    m_uploaded = false;
}
