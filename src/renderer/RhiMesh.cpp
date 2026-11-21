#include "RhiMesh.h"

#include <QSet>
#include <QtMath>

RhiMesh::RhiMesh(QRhi* rhi, const CpuMesh& cpu)
    : m_rhi(rhi), m_baseData(cpu), m_cpuData(cpu),
      m_indexCount(cpu.indices.size())
{
    rebuildDerivedData();
}

RhiMesh::~RhiMesh() {
    releaseGpu();
}

void RhiMesh::setTransform(const QMatrix4x4& transform) {
    m_transform = transform;

    m_cpuData = m_baseData;
    m_cpuData.bbox = BoundingBox();

    const QMatrix4x4 normalMatrix = m_transform.inverted().transposed();
    for (auto& vertex : m_cpuData.vertices) {
        vertex.position = m_transform.map(vertex.position);
        vertex.normal = normalMatrix.mapVector(vertex.normal).normalized();
        m_cpuData.bbox.expand(vertex.position);
    }

    m_indexCount = m_cpuData.indices.size();
    rebuildDerivedData();

    // 顶点数据已变化，旧的 GPU 缓冲需要重新创建
    releaseGpu();
}

void RhiMesh::rebuildDerivedData() {
    buildLineIndices();
    buildNormalLines();
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

void RhiMesh::upload(QRhiResourceUpdateBatch* batch) {
    if (m_uploaded || !m_rhi) {
        return;
    }

    const quint32 vbufSize = m_cpuData.vertices.size() * sizeof(CpuMesh::Vertex);
    const quint32 ibufSize = m_cpuData.indices.size() * sizeof(quint32);

    if (vbufSize > 0 && !m_vbuf) {
        m_vbuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                  QRhiBuffer::VertexBuffer, vbufSize);
        m_vbuf->create();
        batch->uploadStaticBuffer(m_vbuf, m_cpuData.vertices.constData());
    }

    if (ibufSize > 0 && !m_ibuf) {
        m_ibuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                  QRhiBuffer::IndexBuffer, ibufSize);
        m_ibuf->create();
        batch->uploadStaticBuffer(m_ibuf, m_cpuData.indices.constData());
    }

    const quint32 lineIbufSize = m_lineIndices.size() * sizeof(quint32);
    if (lineIbufSize > 0 && !m_lineIbuf) {
        m_lineIbuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                      QRhiBuffer::IndexBuffer, lineIbufSize);
        m_lineIbuf->create();
        batch->uploadStaticBuffer(m_lineIbuf, m_lineIndices.constData());
    }

    const quint32 vnormalSize = m_vertexNormalLines.size() * sizeof(LineVertex);
    if (vnormalSize > 0 && !m_vnormalVbuf) {
        m_vnormalVbuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                         QRhiBuffer::VertexBuffer, vnormalSize);
        m_vnormalVbuf->create();
        batch->uploadStaticBuffer(m_vnormalVbuf, m_vertexNormalLines.constData());
    }

    const quint32 fnormalSize = m_faceNormalLines.size() * sizeof(LineVertex);
    if (fnormalSize > 0 && !m_fnormalVbuf) {
        m_fnormalVbuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                         QRhiBuffer::VertexBuffer, fnormalSize);
        m_fnormalVbuf->create();
        batch->uploadStaticBuffer(m_fnormalVbuf, m_faceNormalLines.constData());
    }

    m_uploaded = true;
}

void RhiMesh::releaseGpu() {
    delete m_vbuf;
    delete m_ibuf;
    delete m_lineIbuf;
    delete m_vnormalVbuf;
    delete m_fnormalVbuf;

    m_vbuf = nullptr;
    m_ibuf = nullptr;
    m_lineIbuf = nullptr;
    m_vnormalVbuf = nullptr;
    m_fnormalVbuf = nullptr;
    m_uploaded = false;
}
