#include "RhiRenderer.h"
#include "RhiMesh.h"
#include <QFile>

RhiRenderer::RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt)
    : m_rhi(rhi), m_rt(rt)
{
}

RhiRenderer::~RhiRenderer() {
    delete m_pipeline;      delete m_srb;
    delete m_frameUBO;      delete m_materialUBO;
    delete m_wireframePipeline; delete m_wireframeSrb;
    delete m_normalsPipeline;  delete m_normalsSrb;  delete m_normalsUBO;
    delete m_faceNormalsPipeline; delete m_faceNormalsSrb; delete m_faceNormalsUBO;
    delete m_vertexNormalsPipeline; delete m_vertexNormalsSrb; delete m_vertexNormalsUBO;
    delete m_flatShadingPipeline; delete m_flatShadingSrb;
    delete m_bgPipeline;    delete m_bgSrb;
    delete m_bgUBO;         delete m_bgVBuf;
}

QShader RhiRenderer::loadShader(const QString& qsbPath) {
    QFile f(qsbPath);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Cannot open shader: %s", qPrintable(qsbPath));
    return QShader::fromSerialized(f.readAll());
}

void RhiRenderer::initialize(QRhiCommandBuffer* cb) {
    m_frameUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                   QRhiBuffer::UniformBuffer,
                                   sizeof(FrameUBOData));
    m_frameUBO->create();
    
    m_materialUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer,
                                      sizeof(MaterialUBOData));
    m_materialUBO->create();
    
    m_bgUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                QRhiBuffer::UniformBuffer,
                                sizeof(BackgroundUBOData));
    m_bgUBO->create();
    
    m_bgVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                 QRhiBuffer::VertexBuffer,
                                 sizeof(float) * 12);
    m_bgVBuf->create();
    
    m_normalsUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                     QRhiBuffer::UniformBuffer,
                                     sizeof(float));
    m_normalsUBO->create();
    
    m_faceNormalsUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                         QRhiBuffer::UniformBuffer,
                                         sizeof(float));
    m_faceNormalsUBO->create();
    
    m_vertexNormalsUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                           QRhiBuffer::UniformBuffer,
                                           sizeof(float));
    m_vertexNormalsUBO->create();
    
    buildPipeline();
    buildWireframePipeline();
    buildNormalsPipeline();
    buildFaceNormalsPipeline();
    buildVertexNormalsPipeline();
    buildFlatShadingPipeline();
    buildBackgroundPipeline();
    
    // 上传背景顶点数据
    static const float bgVerts[] = {
        -1, -1,   1, -1,   -1,  1,
        -1,  1,   1, -1,    1,  1,
    };
    auto* bgBatch = m_rhi->nextResourceUpdateBatch();
    bgBatch->uploadStaticBuffer(m_bgVBuf, bgVerts);
    cb->resourceUpdate(bgBatch);
}

void RhiRenderer::uploadMeshes(QRhiCommandBuffer* cb, const QVector<RhiMesh*>& meshes) {
    if (m_meshesUploaded || meshes.isEmpty())
        return;
    
    auto* batch = m_rhi->nextResourceUpdateBatch();
    for (RhiMesh* mesh : meshes) {
        mesh->setRhi(m_rhi);
        mesh->upload(batch);
    }
    cb->resourceUpdate(batch);
    m_meshesUploaded = true;
}

void RhiRenderer::buildPipeline() {
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });
    
    m_srb = m_rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::FragmentStage,
            m_materialUBO),
    });
    m_srb->create();
    
    m_pipeline = m_rhi->newGraphicsPipeline();
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/phong.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/phong.frag.qsb") },
    });
    
    qDebug() << "Phong vertex shader valid:" << loadShader(":/shaders/phong.vert.qsb").isValid();
    qDebug() << "Phong fragment shader valid:" << loadShader(":/shaders/phong.frag.qsb").isValid();
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    
    m_pipeline->setDepthTest(true);
    m_pipeline->setDepthWrite(true);
    m_pipeline->setDepthOp(QRhiGraphicsPipeline::Less);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::Back);
    m_pipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    bool success = m_pipeline->create();
    qDebug() << "Phong pipeline created:" << success;
}

void RhiRenderer::buildWireframePipeline() {
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });
    
    m_wireframeSrb = m_rhi->newShaderResourceBindings();
    m_wireframeSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
    });
    m_wireframeSrb->create();
    
    m_wireframePipeline = m_rhi->newGraphicsPipeline();
    m_wireframePipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/wireframe.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/wireframe.frag.qsb") },
    });
    m_wireframePipeline->setVertexInputLayout(inputLayout);
    m_wireframePipeline->setShaderResourceBindings(m_wireframeSrb);
    m_wireframePipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    
    m_wireframePipeline->setDepthTest(true);
    m_wireframePipeline->setDepthWrite(true);
    m_wireframePipeline->setDepthOp(QRhiGraphicsPipeline::Less);
    m_wireframePipeline->setCullMode(QRhiGraphicsPipeline::Back);
    m_wireframePipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
    m_wireframePipeline->setTopology(QRhiGraphicsPipeline::Lines);
    m_wireframePipeline->create();
}

void RhiRenderer::buildNormalsPipeline() {
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });
    
    m_normalsUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                     QRhiBuffer::UniformBuffer,
                                     sizeof(float)); // normalLength
    m_normalsUBO->create();
    
    m_normalsSrb = m_rhi->newShaderResourceBindings();
    m_normalsSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
        QRhiShaderResourceBinding::uniformBuffer(
            2, QRhiShaderResourceBinding::VertexStage,
            m_normalsUBO),
    });
    m_normalsSrb->create();
    
    m_normalsPipeline = m_rhi->newGraphicsPipeline();
    m_normalsPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(":/shaders/normals.vert.qsb")},
        {QRhiShaderStage::Fragment, loadShader(":/shaders/normals.frag.qsb")},
    });
    m_normalsPipeline->setVertexInputLayout(inputLayout);
    m_normalsPipeline->setShaderResourceBindings(m_normalsSrb);
    m_normalsPipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    
    m_normalsPipeline->setDepthTest(false);
    m_normalsPipeline->setDepthWrite(false);
    m_normalsPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_normalsPipeline->create();
}

void RhiRenderer::buildBackgroundPipeline() {
    static const float bgVerts[] = {
        -1, -1,   1, -1,   -1,  1,
        -1,  1,   1, -1,    1,  1,
    };
    
    m_bgVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable,
                                 QRhiBuffer::VertexBuffer,
                                 sizeof(bgVerts));
    m_bgVBuf->create();
    
    m_bgUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                QRhiBuffer::UniformBuffer, 32);
    m_bgUBO->create();
    
    m_bgSrb = m_rhi->newShaderResourceBindings();
    m_bgSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::FragmentStage, m_bgUBO),
    });
    m_bgSrb->create();
    
    m_bgPipeline = m_rhi->newGraphicsPipeline();
    m_bgPipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/background.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/background.frag.qsb") },
    });
    
    QRhiVertexInputLayout bgLayout;
    bgLayout.setBindings({ { 2 * sizeof(float) } });
    bgLayout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 } });
    m_bgPipeline->setVertexInputLayout(bgLayout);
    m_bgPipeline->setShaderResourceBindings(m_bgSrb);
    m_bgPipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    m_bgPipeline->setDepthTest(false);
    m_bgPipeline->setDepthWrite(false);
    m_bgPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_bgPipeline->create();
}

void RhiRenderer::buildFaceNormalsPipeline() {
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });
    
    m_faceNormalsSrb = m_rhi->newShaderResourceBindings();
    m_faceNormalsSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
        QRhiShaderResourceBinding::uniformBuffer(
            2, QRhiShaderResourceBinding::GeometryStage,
            m_faceNormalsUBO),
    });
    m_faceNormalsSrb->create();
    
    m_faceNormalsPipeline = m_rhi->newGraphicsPipeline();
    m_faceNormalsPipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/face_normal.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/face_normal.frag.qsb") },
    });
    m_faceNormalsPipeline->setVertexInputLayout(inputLayout);
    m_faceNormalsPipeline->setShaderResourceBindings(m_faceNormalsSrb);
    m_faceNormalsPipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    
    m_faceNormalsPipeline->setDepthTest(true);
    m_faceNormalsPipeline->setDepthWrite(true);
    m_faceNormalsPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_faceNormalsPipeline->create();
}

void RhiRenderer::buildVertexNormalsPipeline() {
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });
    
    m_vertexNormalsSrb = m_rhi->newShaderResourceBindings();
    m_vertexNormalsSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
        QRhiShaderResourceBinding::uniformBuffer(
            2, QRhiShaderResourceBinding::GeometryStage,
            m_vertexNormalsUBO),
    });
    m_vertexNormalsSrb->create();
    
    m_vertexNormalsPipeline = m_rhi->newGraphicsPipeline();
    m_vertexNormalsPipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/vertex_normal.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/vertex_normal.frag.qsb") },
    });
    m_vertexNormalsPipeline->setVertexInputLayout(inputLayout);
    m_vertexNormalsPipeline->setShaderResourceBindings(m_vertexNormalsSrb);
    m_vertexNormalsPipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    
    m_vertexNormalsPipeline->setDepthTest(true);
    m_vertexNormalsPipeline->setDepthWrite(true);
    m_vertexNormalsPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_vertexNormalsPipeline->create();
}

void RhiRenderer::buildFlatShadingPipeline() {
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(CpuMesh::Vertex) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float3, 12 },
        { 0, 2, QRhiVertexInputAttribute::Float2, 24 },
    });
    
    m_flatShadingSrb = m_rhi->newShaderResourceBindings();
    m_flatShadingSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::FragmentStage,
            m_materialUBO),
    });
    m_flatShadingSrb->create();
    
    m_flatShadingPipeline = m_rhi->newGraphicsPipeline();
    m_flatShadingPipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   loadShader(":/shaders/phong.vert.qsb") },
        { QRhiShaderStage::Fragment, loadShader(":/shaders/phong.frag.qsb") },
    });
    m_flatShadingPipeline->setVertexInputLayout(inputLayout);
    m_flatShadingPipeline->setShaderResourceBindings(m_flatShadingSrb);
    m_flatShadingPipeline->setRenderPassDescriptor(m_rt->renderPassDescriptor());
    
    m_flatShadingPipeline->setDepthTest(true);
    m_flatShadingPipeline->setDepthWrite(true);
    m_flatShadingPipeline->setDepthOp(QRhiGraphicsPipeline::Less);
    m_flatShadingPipeline->setCullMode(QRhiGraphicsPipeline::Back);
    m_flatShadingPipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
    m_flatShadingPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_flatShadingPipeline->create();
}

void RhiRenderer::render(QRhiCommandBuffer* cb,
                          const QVector<RhiMesh*>& meshes,
                          const Camera& cam,
                          const Light& light,
                          const Material& mat)
{
    qDebug() << "Rendering" << meshes.size() << "meshes, mode:" << m_renderMode;
    
    // 第一次render时上传mesh数据
    uploadMeshes(cb, meshes);
    
    // 检查mesh缓冲是否有效
    for (RhiMesh* mesh : meshes) {
        if (!mesh->vertexBuffer() || !mesh->indexBuffer()) {
            qWarning() << "Invalid mesh buffers!";
            return;
        }
    }
    
    FrameUBOData frameData;
    const QMatrix4x4 view = cam.viewMatrix();
    const QMatrix4x4 proj = m_rhi->clipSpaceCorrMatrix() * cam.projectionMatrix();
    const QMatrix4x4 model;
    const QMatrix4x4 normalMat = (view * model).inverted().transposed();
    
    memcpy(frameData.model,        model.constData(),     64);
    memcpy(frameData.view,         view.constData(),      64);
    memcpy(frameData.projection,   proj.constData(),      64);
    memcpy(frameData.normalMatrix, normalMat.constData(), 64);
    
    const QVector3D lp = light.position;
    frameData.lightPos[0] = lp.x(); frameData.lightPos[1] = lp.y(); frameData.lightPos[2] = lp.z();
    const QVector3D lc = light.color;
    frameData.lightColor[0] = lc.x(); frameData.lightColor[1] = lc.y(); frameData.lightColor[2] = lc.z();
    const QVector3D vp = cam.position();
    frameData.viewPos[0] = vp.x(); frameData.viewPos[1] = vp.y(); frameData.viewPos[2] = vp.z();
    
    MaterialUBOData matData;
    matData.shininess = mat.shininess;
    memcpy(matData.ambient,  &mat.ambient,  12);
    memcpy(matData.diffuse,  &mat.diffuse,  12);
    memcpy(matData.specular, &mat.specular, 12);
    
    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    updates->updateDynamicBuffer(m_frameUBO,    0, sizeof(FrameUBOData),    &frameData);
    updates->updateDynamicBuffer(m_materialUBO, 0, sizeof(MaterialUBOData), &matData);
    
    // 为normals模式添加normalLength更新
    if (m_renderMode == 2 || m_renderMode == 3 || m_renderMode == 4) {
        float normalLength = 0.1f;
        if (m_renderMode == 2) {
            updates->updateDynamicBuffer(m_normalsUBO, 0, sizeof(float), &normalLength);
        } else if (m_renderMode == 3) {
            updates->updateDynamicBuffer(m_faceNormalsUBO, 0, sizeof(float), &normalLength);
        } else if (m_renderMode == 4) {
            updates->updateDynamicBuffer(m_vertexNormalsUBO, 0, sizeof(float), &normalLength);
        }
    }
    
    // 更新背景颜色
    BackgroundUBOData bgData;
    bgData.topColor[0] = m_bgTopColor.x(); bgData.topColor[1] = m_bgTopColor.y();
    bgData.topColor[2] = m_bgTopColor.z(); bgData.topColor[3] = 1.0f;
    bgData.botColor[0] = m_bgBotColor.x(); bgData.botColor[1] = m_bgBotColor.y();
    bgData.botColor[2] = m_bgBotColor.z(); bgData.botColor[3] = 1.0f;
    updates->updateDynamicBuffer(m_bgUBO, 0, sizeof(BackgroundUBOData), &bgData);
    
    const QColor clearColor(30, 30, 30);
    const QRhiDepthStencilClearValue dsv(1.0f, 0);
    
    cb->beginPass(m_rt, clearColor, dsv, updates);
    
    const QSize sz = m_rt->pixelSize();
    cb->setViewport({ 0, 0, float(sz.width()), float(sz.height()) });
    
    // 绘制背景
    if (m_bgPipeline && m_bgVBuf) {
        cb->setGraphicsPipeline(m_bgPipeline);
        cb->setShaderResources(m_bgSrb);
        const QRhiCommandBuffer::VertexInput bgVBufBind(m_bgVBuf, 0);
        cb->setVertexInput(0, 1, &bgVBufBind);
        cb->draw(6);
    }
    
    // 绘制模型
    if (m_renderMode == 0) {
        // Phong 模式
        cb->setGraphicsPipeline(m_pipeline);
        cb->setShaderResources(m_srb);
        
        for (RhiMesh* mesh : meshes) {
            const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
            cb->setVertexInput(0, 1, &vbufBind,
                               mesh->indexBuffer(),
                               0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(mesh->indexCount());
        }
    } else if (m_renderMode == 1) {
        // Wireframe 模式
        cb->setGraphicsPipeline(m_wireframePipeline);
        cb->setShaderResources(m_wireframeSrb);
        
        for (RhiMesh* mesh : meshes) {
            const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
            cb->setVertexInput(0, 1, &vbufBind,
                               mesh->indexBuffer(),
                               0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(mesh->indexCount());
        }
    } else if (m_renderMode == 2) {
        // Vertex Normals 模式
        cb->setGraphicsPipeline(m_normalsPipeline);
        cb->setShaderResources(m_normalsSrb);
        
        for (RhiMesh* mesh : meshes) {
            const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
            cb->setVertexInput(0, 1, &vbufBind,
                               mesh->indexBuffer(),
                               0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(mesh->indexCount());
        }
    } else if (m_renderMode == 3) {
        // Face Normals 模式
        cb->setGraphicsPipeline(m_faceNormalsPipeline);
        cb->setShaderResources(m_faceNormalsSrb);
        
        for (RhiMesh* mesh : meshes) {
            const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
            cb->setVertexInput(0, 1, &vbufBind,
                               mesh->indexBuffer(),
                               0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(mesh->indexCount());
        }
    } else if (m_renderMode == 4) {
        // Vertex Normals 可视化模式
        cb->setGraphicsPipeline(m_vertexNormalsPipeline);
        cb->setShaderResources(m_vertexNormalsSrb);
        
        for (RhiMesh* mesh : meshes) {
            const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
            cb->setVertexInput(0, 1, &vbufBind,
                               mesh->indexBuffer(),
                               0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(mesh->indexCount());
        }
    } else if (m_renderMode == 5) {
        // Flat Shading 模式
        cb->setGraphicsPipeline(m_flatShadingPipeline);
        cb->setShaderResources(m_flatShadingSrb);
        
        for (RhiMesh* mesh : meshes) {
            const QRhiCommandBuffer::VertexInput vbufBind(mesh->vertexBuffer(), 0);
            cb->setVertexInput(0, 1, &vbufBind,
                               mesh->indexBuffer(),
                               0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(mesh->indexCount());
        }
    }
    
    cb->endPass();
}

void RhiRenderer::setRenderTarget(QRhiRenderTarget* rt) {
    m_rt = rt;
}

void RhiRenderer::releaseAndRebuildPipelines() {
    delete m_pipeline;  m_pipeline  = nullptr;
    delete m_srb;       m_srb       = nullptr;
    delete m_wireframePipeline;  m_wireframePipeline = nullptr;
    delete m_wireframeSrb;       m_wireframeSrb = nullptr;
    delete m_normalsPipeline;    m_normalsPipeline = nullptr;
    delete m_normalsSrb;         m_normalsSrb = nullptr;
    delete m_faceNormalsPipeline; m_faceNormalsPipeline = nullptr;
    delete m_faceNormalsSrb;      m_faceNormalsSrb = nullptr;
    delete m_vertexNormalsPipeline; m_vertexNormalsPipeline = nullptr;
    delete m_vertexNormalsSrb;      m_vertexNormalsSrb = nullptr;
    delete m_flatShadingPipeline; m_flatShadingPipeline = nullptr;
    delete m_flatShadingSrb;      m_flatShadingSrb = nullptr;
    delete m_bgPipeline; m_bgPipeline = nullptr;
    delete m_bgSrb;     m_bgSrb     = nullptr;
    buildPipeline();
    buildWireframePipeline();
    buildNormalsPipeline();
    buildFaceNormalsPipeline();
    buildVertexNormalsPipeline();
    buildFlatShadingPipeline();
    buildBackgroundPipeline();
}

void RhiRenderer::setRenderMode(int mode) {
    m_renderMode = mode;
}

void RhiRenderer::setBackgroundColors(const QVector3D& topColor, const QVector3D& botColor) {
    m_bgTopColor = topColor;
    m_bgBotColor = botColor;
}
