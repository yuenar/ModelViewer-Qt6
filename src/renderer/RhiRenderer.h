#pragma once
#include <rhi/qrhi.h>
#include "../math/Camera.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../core/CpuMesh.h"

class RhiMesh;

struct alignas(16) FrameUBOData {
    float model[16];
    float view[16];
    float projection[16];
    float normalMatrix[16];
    float lightPos[3];   float _pad0;
    float lightColor[3]; float _pad1;
    float viewPos[3];    float _pad2;
};

struct alignas(16) MaterialUBOData {
    float ambient[3];    float shininess;
    float diffuse[3];    float _pad0;
    float specular[3];   float _pad1;
};

class RhiRenderer {
public:
    RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt);
    ~RhiRenderer();
    
    void initialize(QRhiCommandBuffer* cb);
    void setRenderTarget(QRhiRenderTarget* rt);
    void releaseAndRebuildPipelines();
    void setRenderMode(int mode);
    
    void render(QRhiCommandBuffer* cb,
                const QVector<RhiMesh*>& meshes,
                const Camera& cam,
                const Light& light,
                const Material& mat);
    
private:
    void buildPipeline();
    void buildWireframePipeline();
    void buildNormalsPipeline();
    void buildBackgroundPipeline();
    QShader loadShader(const QString& qsbPath);
    
    QRhi* m_rhi;
    QRhiRenderTarget* m_rt;
    
    // Phong 管线
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;
    QRhiBuffer* m_frameUBO = nullptr;
    QRhiBuffer* m_materialUBO = nullptr;
    
    // Wireframe 管线
    QRhiGraphicsPipeline* m_wireframePipeline = nullptr;
    QRhiShaderResourceBindings* m_wireframeSrb = nullptr;
    
    // Normals 管线
    QRhiGraphicsPipeline* m_normalsPipeline = nullptr;
    QRhiShaderResourceBindings* m_normalsSrb = nullptr;
    QRhiBuffer* m_normalsUBO = nullptr;
    
    // Background 管线
    QRhiGraphicsPipeline* m_bgPipeline = nullptr;
    QRhiShaderResourceBindings* m_bgSrb = nullptr;
    QRhiBuffer* m_bgUBO = nullptr;
    QRhiBuffer* m_bgVBuf = nullptr;
    
    int m_renderMode = 0;
};
