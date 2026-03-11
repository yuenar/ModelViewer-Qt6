#pragma once
#include <rhi/qrhi.h>
#include "../core/CpuMesh.h"

class RhiMesh {
public:
    RhiMesh(QRhi* rhi, const CpuMesh& cpu);
    ~RhiMesh();
    
    void upload();
    
    QRhiBuffer* vertexBuffer() const { return m_vbuf; }
    QRhiBuffer* indexBuffer() const { return m_ibuf; }
    int indexCount() const { return m_indexCount; }
    
    QRhiResourceUpdateBatch* pendingBatch() const { return m_pendingBatch; }
    
private:
    QRhi* m_rhi = nullptr;
    QRhiBuffer* m_vbuf = nullptr;
    QRhiBuffer* m_ibuf = nullptr;
    CpuMesh m_cpuData;
    int m_indexCount = 0;
    QRhiResourceUpdateBatch* m_pendingBatch = nullptr;
};
