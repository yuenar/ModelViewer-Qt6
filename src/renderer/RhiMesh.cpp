#include "RhiMesh.h"

RhiMesh::RhiMesh(QRhi* rhi, const CpuMesh& cpu)
    : m_rhi(rhi), m_cpuData(cpu), m_indexCount(cpu.indices.size())
{
}

RhiMesh::~RhiMesh() {
    delete m_vbuf;
    delete m_ibuf;
}

void RhiMesh::upload(QRhiResourceUpdateBatch* batch) {
    const quint32 vbufSize = m_cpuData.vertices.size() * sizeof(CpuMesh::Vertex);
    const quint32 ibufSize = m_cpuData.indices.size() * sizeof(quint32);
    
    m_vbuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                               QRhiBuffer::VertexBuffer, vbufSize);
    m_vbuf->create();
    
    m_ibuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                               QRhiBuffer::IndexBuffer, ibufSize);
    m_ibuf->create();
    
    batch->uploadStaticBuffer(m_vbuf, m_cpuData.vertices.constData());
    batch->uploadStaticBuffer(m_ibuf, m_cpuData.indices.constData());
}
