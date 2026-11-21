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
    rebuildCpuData();
}

RhiMesh::~RhiMesh() {
    releaseGpu();
}

void RhiMesh::setTransform(const QMatrix4x4& transform) {
    m_userTransform = transform;
    m_transform = m_animDelta * m_userTransform;
    rebuildCpuData();
    releaseGpu();
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
    releaseGpu();
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
    copy->rebuildCpuData();
    return copy;
}

void RhiMesh::rebuildCpuData() {
    m_cpuData = m_baseData;

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
    rebuildDerivedData();
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

    const quint32 vbufSize = m_cpuData.vertices.size() * sizeof(CpuMesh::Vertex);
    const quint32 ibufSize = m_cpuData.indices.size() * sizeof(quint32);

    if (vbufSize > 0 && !m_vbuf) {
        m_vbuf = createBuffer(batch, QRhiBuffer::VertexBuffer,
                              m_cpuData.vertices.constData(), vbufSize,
                              &m_vbufCapacity);
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
    m_uploaded = false;
}
