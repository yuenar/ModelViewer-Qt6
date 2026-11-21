#include "RhiRenderer.h"
#include "RhiMesh.h"

#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cstring>

namespace {

struct AxisVertex {
    QVector3D position;
    QVector3D color;
};

struct LabelVertex {
    QVector3D center;
    QVector2D corner;
    QVector2D uv;
    QVector3D color;
    float scale;
};

void copyMatrix(float dst[16], const QMatrix4x4& m) {
    std::memcpy(dst, m.constData(), 16 * sizeof(float));
}

}  // namespace

RhiRenderer::RhiRenderer(QRhi* rhi, QRhiRenderTarget* rt)
    : m_rhi(rhi), m_rt(rt) {
}

RhiRenderer::~RhiRenderer() {
    releaseResources();
}

QShader RhiRenderer::loadShader(const QString& qsbPath) {
    QFile file(qsbPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open shader:" << qsbPath;
        return QShader();
    }
    const QShader shader = QShader::fromSerialized(file.readAll());
    if (!shader.isValid()) {
        qWarning() << "Invalid shader:" << qsbPath;
    }
    return shader;
}

void RhiRenderer::initialize(QRhiCommandBuffer* cb) {
    if (m_initialized || !m_rhi || !m_rt) {
        return;
    }

    createBuffers(cb);
    createShadowResources();
    createSkyboxGeometry(cb);
    createDiffuseTexture();
    createLabelResources(cb);

    if (isPostEnabled()) {
        ensureSceneTarget(m_rt->pixelSize());
        m_pipelineRp = m_sceneRp;
    } else {
        m_pipelineRp = nullptr;
    }
    buildAllPipelines();

    m_diffuseDirty = false;
    m_initialized = true;
}

void RhiRenderer::releaseResources() {
    destroyPipelines();
    destroySceneTarget();
    destroySkyboxGeometry();
    destroyShadowResources();
    destroyDiffuseTexture();
    destroyLabelResources();
    destroyBuffers();
    m_bufferPool.clear();
    m_initialized = false;
    m_meshesUploaded = false;
}

void RhiRenderer::setRenderTarget(QRhiRenderTarget* rt) {
    m_rt = rt;
}

void RhiRenderer::rebuildPipelines() {
    if (!m_initialized) {
        return;
    }
    destroyPipelines();
    buildAllPipelines();
}

QRhiVertexInputLayout RhiRenderer::meshInputLayout() {
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(CpuMesh::Vertex)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float3, 0},
        {0, 1, QRhiVertexInputAttribute::Float3, 12},
        {0, 2, QRhiVertexInputAttribute::Float2, 24},
    });
    return layout;
}

QRhiVertexInputLayout RhiRenderer::lineInputLayout() {
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(RhiMesh::LineVertex)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float3, 0},
        {0, 1, QRhiVertexInputAttribute::Float3, 12},
    });
    return layout;
}

QRhiVertexInputLayout RhiRenderer::backgroundInputLayout() {
    QRhiVertexInputLayout layout;
    layout.setBindings({{2 * sizeof(float)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});
    return layout;
}

QRhiVertexInputLayout RhiRenderer::skyboxInputLayout() {
    QRhiVertexInputLayout layout;
    layout.setBindings({{3 * sizeof(float)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});
    return layout;
}

QRhiVertexInputLayout RhiRenderer::colorVertexInputLayout() {
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(AxisVertex)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float3, 0},
        {0, 1, QRhiVertexInputAttribute::Float3, 12},
    });
    return layout;
}

QRhiGraphicsPipeline* RhiRenderer::createPipeline(const PipelineDesc& desc) {
    if (!m_rhi || !m_rt || !desc.srb) {
        return nullptr;
    }

    QRhiGraphicsPipeline* pipeline = m_rhi->newGraphicsPipeline();
    pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(desc.vertShader)},
        {QRhiShaderStage::Fragment, loadShader(desc.fragShader)},
    });
    pipeline->setVertexInputLayout(desc.layout);
    pipeline->setShaderResourceBindings(desc.srb);
    QRhiRenderPassDescriptor* renderPass =
        desc.renderPass ? desc.renderPass
                        : (m_pipelineRp ? m_pipelineRp
                                        : m_rt->renderPassDescriptor());
    pipeline->setRenderPassDescriptor(renderPass);
    // 离屏场景通道固定为单采样；直接渲染时使用交换链的采样数（MSAA）
    const int sampleCount =
        desc.sampleCount >= 0
            ? desc.sampleCount
            : ((m_pipelineRp && m_pipelineRp == m_sceneRp)
                   ? 1
                   : m_rt->sampleCount());
    pipeline->setSampleCount(sampleCount);
    pipeline->setTopology(desc.topology);
    pipeline->setDepthTest(desc.depthTest);
    pipeline->setDepthWrite(desc.depthWrite);
    pipeline->setDepthOp(desc.depthOp);
    pipeline->setCullMode(desc.cull);
    pipeline->setFrontFace(desc.frontFace);

    if (!pipeline->create()) {
        qWarning() << "Failed to create pipeline:" << desc.vertShader << desc.fragShader;
        delete pipeline;
        return nullptr;
    }
    return pipeline;
}

void RhiRenderer::createBuffers(QRhiCommandBuffer* cb) {
    m_bufferPool.setRhi(m_rhi);
    m_frameStride = int(m_rhi->ubufAligned(sizeof(FrameUBOData)));
    m_materialStride = int(m_rhi->ubufAligned(sizeof(MaterialUBOData)));

    m_frameUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                  quint32(m_frameStride * m_frameSlots));
    m_materialUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                     quint32(m_materialStride * m_materialSlots));
    m_bgUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                               sizeof(BackgroundUBOData));
    m_normalUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                   sizeof(NormalUBOData));
    m_clipUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                 sizeof(ClipUBOData));
    m_selectionUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                      sizeof(SelectionUBOData));
    m_shadowUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                   sizeof(ShadowUBOData));
    m_skyboxUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                   sizeof(SkyboxUBOData));
    m_bgVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                6 * 2 * sizeof(float));
    m_postUBO = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                 sizeof(PostUBOData));
    m_postVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                  6 * 2 * sizeof(float));

    m_frameUBO->create();
    m_materialUBO->create();
    m_bgUBO->create();
    m_normalUBO->create();
    m_clipUBO->create();
    m_selectionUBO->create();
    m_shadowUBO->create();
    m_skyboxUBO->create();
    m_bgVBuf->create();
    m_postUBO->create();
    m_postVBuf->create();

    static const float bgVerts[] = {
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, 1.0f,
    };
    static const float postVerts[] = {
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, 1.0f,
    };

    if (cb) {
        auto* batch = m_rhi->nextResourceUpdateBatch();
        batch->uploadStaticBuffer(m_bgVBuf, bgVerts);
        batch->uploadStaticBuffer(m_postVBuf, postVerts);
        cb->resourceUpdate(batch);
    }
}

void RhiRenderer::destroyBuffers() {
    delete m_frameUBO;
    delete m_materialUBO;
    delete m_bgUBO;
    delete m_normalUBO;
    delete m_clipUBO;
    delete m_selectionUBO;
    delete m_shadowUBO;
    delete m_skyboxUBO;
    delete m_bgVBuf;
    delete m_axisVBuf;
    delete m_gridVBuf;
    delete m_capVBuf;
    delete m_labelVBuf;
    delete m_postUBO;
    delete m_postVBuf;

    m_frameUBO = nullptr;
    m_materialUBO = nullptr;
    m_bgUBO = nullptr;
    m_normalUBO = nullptr;
    m_clipUBO = nullptr;
    m_selectionUBO = nullptr;
    m_shadowUBO = nullptr;
    m_skyboxUBO = nullptr;
    m_bgVBuf = nullptr;
    m_axisVBuf = nullptr;
    m_gridVBuf = nullptr;
    m_capVBuf = nullptr;
    m_labelVBuf = nullptr;
    m_postUBO = nullptr;
    m_postVBuf = nullptr;
    m_axisVertexCount = 0;
    m_gridVertexCount = 0;
    m_capVertexCount = 0;
    m_labelVertexCount = 0;
}

void RhiRenderer::createShadowResources() {
    constexpr int kShadowSize = 2048;

    m_shadowTexture = m_rhi->newTexture(
        QRhiTexture::D32F, QSize(kShadowSize, kShadowSize), 1,
        QRhiTexture::RenderTarget);
    if (!m_shadowTexture->create()) {
        qWarning() << "Failed to create shadow texture";
        delete m_shadowTexture;
        m_shadowTexture = nullptr;
        return;
    }

    m_shadowSampler = m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                        QRhiSampler::None, QRhiSampler::ClampToEdge,
                                        QRhiSampler::ClampToEdge);
    m_shadowSampler->setTextureCompareOp(QRhiSampler::Less);
    m_shadowSampler->create();

    QRhiTextureRenderTargetDescription rtDesc;
    rtDesc.setDepthTexture(m_shadowTexture);
    m_shadowRt = m_rhi->newTextureRenderTarget(rtDesc);
    m_shadowRp = m_shadowRt->newCompatibleRenderPassDescriptor();
    m_shadowRt->setRenderPassDescriptor(m_shadowRp);
    if (!m_shadowRt->create()) {
        qWarning() << "Failed to create shadow render target";
        destroyShadowResources();
    }
}

void RhiRenderer::destroyShadowResources() {
    delete m_shadowRt;
    delete m_shadowRp;
    delete m_shadowSampler;
    delete m_shadowTexture;
    m_shadowRt = nullptr;
    m_shadowRp = nullptr;
    m_shadowSampler = nullptr;
    m_shadowTexture = nullptr;
}

void RhiRenderer::createSkyboxGeometry(QRhiCommandBuffer* cb) {
    static const float skyboxVerts[] = {
        -1, -1, -1,   1, -1, -1,   1,  1, -1,
        -1, -1, -1,   1,  1, -1,  -1,  1, -1,
        -1, -1,  1,   1,  1,  1,   1, -1,  1,
        -1, -1,  1,  -1,  1,  1,   1,  1,  1,
        -1, -1, -1,  -1,  1, -1,  -1,  1,  1,
        -1, -1, -1,  -1,  1,  1,  -1, -1,  1,
         1, -1, -1,   1,  1,  1,   1,  1, -1,
         1, -1, -1,   1, -1,  1,   1,  1,  1,
        -1,  1, -1,   1,  1, -1,   1,  1,  1,
        -1,  1, -1,   1,  1,  1,  -1,  1,  1,
        -1, -1, -1,  -1, -1,  1,   1, -1,  1,
        -1, -1, -1,   1, -1,  1,   1, -1, -1,
    };

    m_skyboxVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                    sizeof(skyboxVerts));
    m_skyboxVBuf->create();

    if (cb) {
        auto* batch = m_rhi->nextResourceUpdateBatch();
        batch->uploadStaticBuffer(m_skyboxVBuf, skyboxVerts);
        cb->resourceUpdate(batch);
    }
}

void RhiRenderer::destroySkyboxGeometry() {
    delete m_skyboxVBuf;
    m_skyboxVBuf = nullptr;
}

void RhiRenderer::createDiffuseTexture() {
    destroyDiffuseTexture();

    QImage image = m_diffuseImage;
    if (image.isNull()) {
        image = QImage(1, 1, QImage::Format_RGBA8888);
        image.fill(Qt::white);
    } else {
        image = image.convertToFormat(QImage::Format_RGBA8888);
    }

    m_diffuseTexture = m_rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1);
    m_diffuseTexture->create();

    m_diffuseSampler = m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                         QRhiSampler::Linear, QRhiSampler::Repeat,
                                         QRhiSampler::Repeat);
    m_diffuseSampler->create();

    m_diffuseUploadPending = true;
}

void RhiRenderer::destroyDiffuseTexture() {
    delete m_diffuseSampler;
    delete m_diffuseTexture;
    m_diffuseSampler = nullptr;
    m_diffuseTexture = nullptr;
    m_diffuseUploadPending = false;
}

void RhiRenderer::setDiffuseTexture(const QImage& image) {
    m_diffuseImage = image;
    m_diffuseDirty = true;
}

void RhiRenderer::setPostProcessing(bool fxaa, bool ao, float aoStrength) {
    m_aoStrength = aoStrength;
    if (m_fxaaEnabled == fxaa && m_aoEnabled == ao) {
        return;
    }

    m_fxaaEnabled = fxaa;
    m_aoEnabled = ao;

    if (!m_initialized || !m_rhi || !m_rt) {
        return;
    }

    destroyPipelines();
    if (isPostEnabled()) {
        ensureSceneTarget(m_rt->pixelSize());
        m_pipelineRp = m_sceneRp;
    } else {
        m_pipelineRp = nullptr;
        destroySceneTarget();
    }
    buildAllPipelines();
}

void RhiRenderer::ensureSceneTarget(const QSize& size) {
    if (!m_rhi || size.isEmpty()) {
        return;
    }
    if (m_sceneRt && m_sceneSize == size && m_sceneColor && m_sceneDepth) {
        return;
    }

    destroySceneTarget();

    m_sceneColor = m_rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                     QRhiTexture::RenderTarget);
    m_sceneDepth = m_rhi->newTexture(QRhiTexture::D32F, size, 1,
                                     QRhiTexture::RenderTarget);

    if (!m_sceneColor->create() || !m_sceneDepth->create()) {
        qWarning() << "Failed to create post-processing scene textures";
        destroySceneTarget();
        return;
    }

    m_sceneColorSampler = m_rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sceneDepthSampler = m_rhi->newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sceneColorSampler->create();
    m_sceneDepthSampler->create();

    QRhiTextureRenderTargetDescription rtDesc;
    rtDesc.setColorAttachments({QRhiColorAttachment(m_sceneColor)});
    rtDesc.setDepthTexture(m_sceneDepth);

    m_sceneRt = m_rhi->newTextureRenderTarget(rtDesc);
    m_sceneRp = m_sceneRt->newCompatibleRenderPassDescriptor();
    m_sceneRt->setRenderPassDescriptor(m_sceneRp);
    if (!m_sceneRt->create()) {
        qWarning() << "Failed to create post-processing render target";
        destroySceneTarget();
        return;
    }

    m_sceneSize = size;
}

void RhiRenderer::destroySceneTarget() {
    delete m_sceneRt;
    delete m_sceneRp;
    delete m_sceneColorSampler;
    delete m_sceneDepthSampler;
    delete m_sceneColor;
    delete m_sceneDepth;

    m_sceneRt = nullptr;
    m_sceneRp = nullptr;
    m_sceneColorSampler = nullptr;
    m_sceneDepthSampler = nullptr;
    m_sceneColor = nullptr;
    m_sceneDepth = nullptr;
    m_sceneSize = QSize();
}

void RhiRenderer::createPostResources() {
    if (!isPostEnabled() || !m_sceneRt || !m_postUBO ||
        !m_sceneColor || !m_sceneDepth) {
        return;
    }

    m_postSrb = m_rhi->newShaderResourceBindings();
    m_postSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage |
                   QRhiShaderResourceBinding::FragmentStage,
            m_postUBO),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, m_sceneColor,
            m_sceneColorSampler),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, m_sceneDepth,
            m_sceneDepthSampler),
    });
    m_postSrb->create();

    PipelineDesc desc;
    desc.vertShader = ":/shaders/post.vert.qsb";
    desc.fragShader = ":/shaders/post.frag.qsb";
    desc.layout = backgroundInputLayout();
    desc.srb = m_postSrb;
    desc.depthTest = false;
    desc.depthWrite = false;
    desc.cull = QRhiGraphicsPipeline::None;
    desc.renderPass = m_rt->renderPassDescriptor();
    desc.sampleCount = qMax(1, m_rt->sampleCount());
    m_postPipeline = createPipeline(desc);
}

void RhiRenderer::destroyPostResources() {
    delete m_postPipeline;
    delete m_postSrb;
    m_postPipeline = nullptr;
    m_postSrb = nullptr;
}

void RhiRenderer::drawPost(QRhiCommandBuffer* cb) {
    if (!m_postPipeline || !m_postSrb || !m_postVBuf) {
        return;
    }
    cb->setGraphicsPipeline(m_postPipeline);
    cb->setShaderResources(m_postSrb);
    const QRhiCommandBuffer::VertexInput binding(m_postVBuf, 0);
    cb->setVertexInput(0, 1, &binding);
    cb->draw(6);
}

void RhiRenderer::createLabelResources(QRhiCommandBuffer* cb) {
    QImage atlas(384, 128, QImage::Format_RGBA8888);
    atlas.fill(Qt::transparent);
    {
        QPainter painter(&atlas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QFont font("Arial", 84, QFont::Bold);
        painter.setFont(font);

        painter.setPen(QColor(230, 70, 70));
        painter.drawText(QRect(0, 0, 128, 128), Qt::AlignCenter, "X");
        painter.setPen(QColor(70, 210, 90));
        painter.drawText(QRect(128, 0, 128, 128), Qt::AlignCenter, "Y");
        painter.setPen(QColor(80, 140, 240));
        painter.drawText(QRect(256, 0, 128, 128), Qt::AlignCenter, "Z");
    }

    m_labelTexture = m_rhi->newTexture(QRhiTexture::RGBA8, atlas.size(), 1);
    m_labelTexture->create();

    m_labelSampler = m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                       QRhiSampler::Linear, QRhiSampler::ClampToEdge,
                                       QRhiSampler::ClampToEdge);
    m_labelSampler->create();

    if (cb) {
        auto* batch = m_rhi->nextResourceUpdateBatch();
        batch->uploadTexture(m_labelTexture, atlas);
        cb->resourceUpdate(batch);
    }
}

void RhiRenderer::destroyLabelResources() {
    delete m_labelSampler;
    delete m_labelTexture;
    m_labelSampler = nullptr;
    m_labelTexture = nullptr;
}

void RhiRenderer::buildAllPipelines() {
    // ── 背景 ────────────────────────────────────────────────
    m_bgSrb = m_rhi->newShaderResourceBindings();
    m_bgSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage,
                                                 m_bgUBO),
    });
    m_bgSrb->create();

    PipelineDesc bgDesc;
    bgDesc.vertShader = ":/shaders/background.vert.qsb";
    bgDesc.fragShader = ":/shaders/background.frag.qsb";
    bgDesc.layout = backgroundInputLayout();
    bgDesc.srb = m_bgSrb;
    bgDesc.depthTest = false;
    bgDesc.depthWrite = false;
    bgDesc.cull = QRhiGraphicsPipeline::None;
    m_bgPipeline = createPipeline(bgDesc);

    // ── Phong ──────────────────────────────────────────────
    m_phongSrb = m_rhi->newShaderResourceBindings();
    m_phongSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, QRhiShaderResourceBinding::FragmentStage, m_materialUBO, sizeof(MaterialUBOData)),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_diffuseTexture, m_diffuseSampler),
    });
    m_phongSrb->create();

    PipelineDesc phongDesc;
    phongDesc.vertShader = ":/shaders/phong.vert.qsb";
    phongDesc.fragShader = ":/shaders/phong.frag.qsb";
    phongDesc.layout = meshInputLayout();
    phongDesc.srb = m_phongSrb;
    m_phongPipeline = createPipeline(phongDesc);

    // ── 线框 ────────────────────────────────────────────────
    m_wireframeSrb = m_rhi->newShaderResourceBindings();
    m_wireframeSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
    });
    m_wireframeSrb->create();

    PipelineDesc wireDesc;
    wireDesc.vertShader = ":/shaders/wireframe.vert.qsb";
    wireDesc.fragShader = ":/shaders/wireframe.frag.qsb";
    wireDesc.layout = meshInputLayout();
    wireDesc.srb = m_wireframeSrb;
    wireDesc.topology = QRhiGraphicsPipeline::Lines;
    wireDesc.cull = QRhiGraphicsPipeline::None;
    m_wireframePipeline = createPipeline(wireDesc);

    PipelineDesc wireOverlayDesc = wireDesc;
    wireOverlayDesc.fragShader = ":/shaders/wire_overlay.frag.qsb";
    wireOverlayDesc.depthTest = false;
    wireOverlayDesc.depthWrite = false;
    m_wireOverlayPipeline = createPipeline(wireOverlayDesc);

    // ── 法线（RGB 着色）────────────────────────────────────
    m_normalsSrb = m_rhi->newShaderResourceBindings();
    m_normalsSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
    });
    m_normalsSrb->create();

    PipelineDesc normalsDesc;
    normalsDesc.vertShader = ":/shaders/normals.vert.qsb";
    normalsDesc.fragShader = ":/shaders/normals.frag.qsb";
    normalsDesc.layout = meshInputLayout();
    normalsDesc.srb = m_normalsSrb;
    m_normalsPipeline = createPipeline(normalsDesc);

    // ── 平面着色 ────────────────────────────────────────────
    m_flatSrb = m_rhi->newShaderResourceBindings();
    m_flatSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, QRhiShaderResourceBinding::FragmentStage, m_materialUBO, sizeof(MaterialUBOData)),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_diffuseTexture, m_diffuseSampler),
    });
    m_flatSrb->create();

    PipelineDesc flatDesc;
    flatDesc.vertShader = ":/shaders/flat.vert.qsb";
    flatDesc.fragShader = ":/shaders/flat.frag.qsb";
    flatDesc.layout = meshInputLayout();
    flatDesc.srb = m_flatSrb;
    m_flatPipeline = createPipeline(flatDesc);

    // ── PBR ────────────────────────────────────────────────
    m_pbrSrb = m_rhi->newShaderResourceBindings();
    m_pbrSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, QRhiShaderResourceBinding::FragmentStage, m_materialUBO, sizeof(MaterialUBOData)),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_diffuseTexture, m_diffuseSampler),
    });
    m_pbrSrb->create();

    PipelineDesc pbrDesc;
    pbrDesc.vertShader = ":/shaders/pbr.vert.qsb";
    pbrDesc.fragShader = ":/shaders/pbr.frag.qsb";
    pbrDesc.layout = meshInputLayout();
    pbrDesc.srb = m_pbrSrb;
    m_pbrPipeline = createPipeline(pbrDesc);

    // ── 法线线段 ────────────────────────────────────────────
    m_normalLinesSrb = m_rhi->newShaderResourceBindings();
    m_normalLinesSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage, m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::uniformBuffer(
            2, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_normalUBO),
    });
    m_normalLinesSrb->create();

    PipelineDesc normalLinesDesc;
    normalLinesDesc.vertShader = ":/shaders/normal_lines.vert.qsb";
    normalLinesDesc.fragShader = ":/shaders/normal_lines.frag.qsb";
    normalLinesDesc.layout = lineInputLayout();
    normalLinesDesc.srb = m_normalLinesSrb;
    normalLinesDesc.topology = QRhiGraphicsPipeline::Lines;
    normalLinesDesc.depthWrite = false;
    normalLinesDesc.depthOp = QRhiGraphicsPipeline::LessOrEqual;
    normalLinesDesc.cull = QRhiGraphicsPipeline::None;
    m_normalLinesPipeline = createPipeline(normalLinesDesc);

    // ── 坐标轴 / 网格 ───────────────────────────────────────
    m_axisSrb = m_rhi->newShaderResourceBindings();
    m_axisSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
    });
    m_axisSrb->create();

    PipelineDesc axisDesc;
    axisDesc.vertShader = ":/shaders/axis.vert.qsb";
    axisDesc.fragShader = ":/shaders/axis.frag.qsb";
    axisDesc.layout = colorVertexInputLayout();
    axisDesc.srb = m_axisSrb;
    axisDesc.topology = QRhiGraphicsPipeline::Lines;
    axisDesc.depthWrite = false;
    axisDesc.depthOp = QRhiGraphicsPipeline::LessOrEqual;
    axisDesc.cull = QRhiGraphicsPipeline::None;
    m_axisPipeline = createPipeline(axisDesc);

    // ── 阴影 ────────────────────────────────────────────────
    if (m_shadowTexture && m_shadowRt && m_shadowRp) {
        m_shadowSrb = m_rhi->newShaderResourceBindings();
        m_shadowSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                     m_shadowUBO),
        });
        m_shadowSrb->create();

        PipelineDesc shadowDesc;
        shadowDesc.vertShader = ":/shaders/shadow_depth.vert.qsb";
        shadowDesc.fragShader = ":/shaders/shadow_depth.frag.qsb";
        shadowDesc.layout = meshInputLayout();
        shadowDesc.srb = m_shadowSrb;
        shadowDesc.depthOp = QRhiGraphicsPipeline::LessOrEqual;
        shadowDesc.cull = QRhiGraphicsPipeline::None;
        shadowDesc.renderPass = m_shadowRp;
        shadowDesc.sampleCount = 1;
        m_shadowPipeline = createPipeline(shadowDesc);

        m_shadowedPhongSrb = m_rhi->newShaderResourceBindings();
        m_shadowedPhongSrb->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                m_frameUBO, sizeof(FrameUBOData)),
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                1, QRhiShaderResourceBinding::FragmentStage, m_materialUBO, sizeof(MaterialUBOData)),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                      m_shadowTexture, m_shadowSampler),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage,
                                                      m_diffuseTexture, m_diffuseSampler),
        });
        m_shadowedPhongSrb->create();

        PipelineDesc shadowedDesc;
        shadowedDesc.vertShader = ":/shaders/shadowed_phong.vert.qsb";
        shadowedDesc.fragShader = ":/shaders/shadowed_phong.frag.qsb";
        shadowedDesc.layout = meshInputLayout();
        shadowedDesc.srb = m_shadowedPhongSrb;
        m_shadowedPhongPipeline = createPipeline(shadowedDesc);
    }

    // ── 天空盒 ──────────────────────────────────────────────
    m_skyboxSrb = m_rhi->newShaderResourceBindings();
    m_skyboxSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                 m_skyboxUBO),
    });
    m_skyboxSrb->create();

    PipelineDesc skyboxDesc;
    skyboxDesc.vertShader = ":/shaders/skybox.vert.qsb";
    skyboxDesc.fragShader = ":/shaders/skybox.frag.qsb";
    skyboxDesc.layout = skyboxInputLayout();
    skyboxDesc.srb = m_skyboxSrb;
    skyboxDesc.depthWrite = false;
    skyboxDesc.depthOp = QRhiGraphicsPipeline::LessOrEqual;
    skyboxDesc.cull = QRhiGraphicsPipeline::None;
    m_skyboxPipeline = createPipeline(skyboxDesc);

    // ── 选择高亮 ────────────────────────────────────────────
    m_selectionSrb = m_rhi->newShaderResourceBindings();
    m_selectionSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage, m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage,
                                                 m_selectionUBO),
    });
    m_selectionSrb->create();

    PipelineDesc selectionDesc;
    selectionDesc.vertShader = ":/shaders/selection.vert.qsb";
    selectionDesc.fragShader = ":/shaders/selection.frag.qsb";
    selectionDesc.layout = meshInputLayout();
    selectionDesc.srb = m_selectionSrb;
    selectionDesc.depthWrite = false;
    selectionDesc.depthOp = QRhiGraphicsPipeline::LessOrEqual;
    selectionDesc.cull = QRhiGraphicsPipeline::None;
    m_selectionPipeline = createPipeline(selectionDesc);

    // ── 裁剪平面 ────────────────────────────────────────────
    m_clippedSrb = m_rhi->newShaderResourceBindings();
    m_clippedSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, QRhiShaderResourceBinding::FragmentStage, m_materialUBO, sizeof(MaterialUBOData)),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_diffuseTexture, m_diffuseSampler),
        QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage,
                                                 m_clipUBO),
    });
    m_clippedSrb->create();

    PipelineDesc clippedDesc;
    clippedDesc.vertShader = ":/shaders/clipped_mesh.vert.qsb";
    clippedDesc.fragShader = ":/shaders/clipped_mesh.frag.qsb";
    clippedDesc.layout = meshInputLayout();
    clippedDesc.srb = m_clippedSrb;
    clippedDesc.cull = QRhiGraphicsPipeline::None;
    m_clippedPipeline = createPipeline(clippedDesc);

    // ── 剖面封盖 ────────────────────────────────────────────
    m_capSrb = m_rhi->newShaderResourceBindings();
    m_capSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
    });
    m_capSrb->create();

    QRhiVertexInputLayout capLayout;
    capLayout.setBindings({{3 * sizeof(float)}});
    capLayout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});

    PipelineDesc capDesc;
    capDesc.vertShader = ":/shaders/cap.vert.qsb";
    capDesc.fragShader = ":/shaders/cap.frag.qsb";
    capDesc.layout = capLayout;
    capDesc.srb = m_capSrb;
    capDesc.cull = QRhiGraphicsPipeline::None;
    m_capPipeline = createPipeline(capDesc);

    // ── 3D 轴标签 ───────────────────────────────────────────
    m_labelSrb = m_rhi->newShaderResourceBindings();
    m_labelSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_frameUBO, sizeof(FrameUBOData)),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                  m_labelTexture, m_labelSampler),
    });
    m_labelSrb->create();

    QRhiVertexInputLayout labelLayout;
    labelLayout.setBindings({{sizeof(LabelVertex)}});
    labelLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float3, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 12},
        {0, 2, QRhiVertexInputAttribute::Float2, 20},
        {0, 3, QRhiVertexInputAttribute::Float3, 28},
        {0, 4, QRhiVertexInputAttribute::Float, 40},
    });

    PipelineDesc labelDesc;
    labelDesc.vertShader = ":/shaders/text3d.vert.qsb";
    labelDesc.fragShader = ":/shaders/text3d.frag.qsb";
    labelDesc.layout = labelLayout;
    labelDesc.srb = m_labelSrb;
    labelDesc.depthWrite = false;
    labelDesc.depthOp = QRhiGraphicsPipeline::LessOrEqual;
    labelDesc.cull = QRhiGraphicsPipeline::None;
    m_labelPipeline = createPipeline(labelDesc);

    createPostResources();
}

void RhiRenderer::destroyPipelines() {
    delete m_bgPipeline; m_bgPipeline = nullptr;
    delete m_bgSrb; m_bgSrb = nullptr;
    delete m_phongPipeline; m_phongPipeline = nullptr;
    delete m_phongSrb; m_phongSrb = nullptr;
    delete m_wireframePipeline; m_wireframePipeline = nullptr;
    delete m_wireOverlayPipeline; m_wireOverlayPipeline = nullptr;
    delete m_wireframeSrb; m_wireframeSrb = nullptr;
    delete m_normalsPipeline; m_normalsPipeline = nullptr;
    delete m_normalsSrb; m_normalsSrb = nullptr;
    delete m_flatPipeline; m_flatPipeline = nullptr;
    delete m_flatSrb; m_flatSrb = nullptr;
    delete m_pbrPipeline; m_pbrPipeline = nullptr;
    delete m_pbrSrb; m_pbrSrb = nullptr;
    delete m_normalLinesPipeline; m_normalLinesPipeline = nullptr;
    delete m_normalLinesSrb; m_normalLinesSrb = nullptr;
    delete m_axisPipeline; m_axisPipeline = nullptr;
    delete m_axisSrb; m_axisSrb = nullptr;
    delete m_shadowPipeline; m_shadowPipeline = nullptr;
    delete m_shadowSrb; m_shadowSrb = nullptr;
    delete m_shadowedPhongPipeline; m_shadowedPhongPipeline = nullptr;
    delete m_shadowedPhongSrb; m_shadowedPhongSrb = nullptr;
    delete m_skyboxPipeline; m_skyboxPipeline = nullptr;
    delete m_skyboxSrb; m_skyboxSrb = nullptr;
    delete m_selectionPipeline; m_selectionPipeline = nullptr;
    delete m_selectionSrb; m_selectionSrb = nullptr;
    delete m_clippedPipeline; m_clippedPipeline = nullptr;
    delete m_clippedSrb; m_clippedSrb = nullptr;
    delete m_capPipeline; m_capPipeline = nullptr;
    delete m_capSrb; m_capSrb = nullptr;
    delete m_labelPipeline; m_labelPipeline = nullptr;
    delete m_labelSrb; m_labelSrb = nullptr;
    delete m_postPipeline; m_postPipeline = nullptr;
    delete m_postSrb; m_postSrb = nullptr;
}

void RhiRenderer::setRenderMode(int mode) {
    if (mode < 0 || mode > 11) {
        return;
    }
    m_renderMode = static_cast<RenderMode>(mode);
}

void RhiRenderer::setDisplayStyle(int style) {
    if (style < 0 || style > 3) {
        return;
    }
    m_displayStyle = static_cast<DisplayStyle>(style);
}

void RhiRenderer::setMultiView(bool enabled) {
    m_multiView = enabled;
}

void RhiRenderer::setAxisVisible(bool visible) {
    m_axisVisible = visible;
}

void RhiRenderer::setGridVisible(bool visible) {
    m_gridVisible = visible;
}

void RhiRenderer::setBackgroundColors(const QVector3D& topColor,
                                      const QVector3D& botColor) {
    m_bgTopColor = topColor;
    m_bgBotColor = botColor;
}

void RhiRenderer::setNormalLength(float length) {
    m_normalLength = qBound(0.0001f, length, 1.0e6f);
}

void RhiRenderer::setClipPlane(const QVector4D& plane) {
    m_clipPlane = plane;
    m_capDirty = true;
}

void RhiRenderer::resetMeshUploadFlag() {
    m_meshesUploaded = false;
    m_axisDirty = true;
    m_capDirty = true;
}

void RhiRenderer::updateSceneBounds(const QVector<RhiMesh*>& meshes) {
    BoundingBox bbox;
    for (const RhiMesh* mesh : meshes) {
        if (mesh) {
            bbox.expand(mesh->cpuMesh().bbox);
        }
    }
    if (bbox.isEmpty()) {
        m_sceneCenter = QVector3D(0, 0, 0);
        m_sceneRadius = 1.0f;
        m_sceneMin = QVector3D(-1, -1, -1);
    } else {
        m_sceneCenter = bbox.center();
        m_sceneRadius = qMax(bbox.radius(), 0.001f);
        m_sceneMin = bbox.min();
    }
}

void RhiRenderer::uploadMeshes(QRhiCommandBuffer* cb,
                               const QVector<RhiMesh*>& meshes) {
    if (meshes.isEmpty()) {
        return;
    }

    if (!m_meshesUploaded) {
        auto* batch = m_rhi->nextResourceUpdateBatch();
        for (RhiMesh* mesh : meshes) {
            if (!mesh) {
                continue;
            }
            mesh->setRhi(m_rhi);
            mesh->setBufferPool(&m_bufferPool);
            mesh->upload(batch);
        }
        cb->resourceUpdate(batch);
        m_meshesUploaded = true;
        m_axisDirty = true;
    }

    updateSceneBounds(meshes);
}

void RhiRenderer::uploadAxisGrid(QRhiResourceUpdateBatch* batch) {
    QVector<AxisVertex> grid;
    QVector<AxisVertex> axis;

    const float extent = qMax(m_sceneRadius * 2.0f, 1.0f);
    const int divisions = 10;
    const float step = (extent * 2.0f) / float(divisions);
    const float y = m_sceneMin.y() - 0.001f;
    const QVector3D gridColor(0.35f, 0.35f, 0.38f);

    for (int i = 0; i <= divisions; ++i) {
        const float x = -extent + float(i) * step;
        grid.append({{x, y, -extent}, gridColor});
        grid.append({{x, y, extent}, gridColor});
        const float z = -extent + float(i) * step;
        grid.append({{-extent, y, z}, gridColor});
        grid.append({{extent, y, z}, gridColor});
    }

    const float axisLength = qMax(m_sceneRadius * 1.05f, 1.0f);
    axis.append({{0, 0, 0}, {0.9f, 0.25f, 0.25f}});
    axis.append({{axisLength, 0, 0}, {0.9f, 0.25f, 0.25f}});
    axis.append({{0, 0, 0}, {0.3f, 0.85f, 0.35f}});
    axis.append({{0, axisLength, 0}, {0.3f, 0.85f, 0.35f}});
    axis.append({{0, 0, 0}, {0.3f, 0.55f, 0.95f}});
    axis.append({{0, 0, axisLength}, {0.3f, 0.55f, 0.95f}});

    m_axisVertexCount = axis.size();
    m_gridVertexCount = grid.size();

    delete m_gridVBuf;
    m_gridVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                  quint32(grid.size() * sizeof(AxisVertex)));
    m_gridVBuf->create();
    batch->uploadStaticBuffer(m_gridVBuf, grid.constData());

    delete m_axisVBuf;
    m_axisVBuf = m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                  quint32(axis.size() * sizeof(AxisVertex)));
    m_axisVBuf->create();
    batch->uploadStaticBuffer(m_axisVBuf, axis.constData());

    // 轴末端的 X/Y/Z 文字标签（公告板四边形）
    QVector<LabelVertex> labels;
    const float labelScale = qMax(m_sceneRadius * 0.12f, 0.15f);

    auto addLabel = [&labels, labelScale](const QVector3D& center, int cell,
                                          const QVector3D& color) {
        const float u0 = float(cell) / 3.0f;
        const float u1 = float(cell + 1) / 3.0f;
        const QVector2D corners[4] = {
            {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        const QVector2D uvs[4] = {
            {u0, 1}, {u1, 1}, {u1, 0}, {u0, 0}};
        const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int i : order) {
            labels.append({center, corners[i], uvs[i], color, labelScale});
        }
    };

    addLabel({axisLength, 0, 0}, 0, {0.9f, 0.3f, 0.3f});
    addLabel({0, axisLength, 0}, 1, {0.3f, 0.85f, 0.35f});
    addLabel({0, 0, axisLength}, 2, {0.35f, 0.55f, 0.95f});

    delete m_labelVBuf;
    m_labelVBuf = m_rhi->newBuffer(
        QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
        quint32(labels.size() * sizeof(LabelVertex)));
    m_labelVBuf->create();
    batch->uploadStaticBuffer(m_labelVBuf, labels.constData());
    m_labelVertexCount = labels.size();

    m_axisVertexCount = axis.size();
    m_axisDirty = false;
}

void RhiRenderer::drawAxisGrid(QRhiCommandBuffer* cb, int slot) {
    if (!m_axisPipeline || (!m_axisVisible && !m_gridVisible)) {
        return;
    }

    const auto offset = frameOffset(slot);
    cb->setGraphicsPipeline(m_axisPipeline);
    cb->setShaderResources(m_axisSrb, 1, &offset);

    if (m_gridVisible && m_gridVBuf && m_gridVertexCount > 0) {
        const QRhiCommandBuffer::VertexInput binding(m_gridVBuf, 0);
        cb->setVertexInput(0, 1, &binding);
        cb->draw(m_gridVertexCount);
    }
    if (m_axisVisible && m_axisVBuf && m_axisVertexCount > 0) {
        const QRhiCommandBuffer::VertexInput binding(m_axisVBuf, 0);
        cb->setVertexInput(0, 1, &binding);
        cb->draw(m_axisVertexCount);
    }
}

void RhiRenderer::rebuildCapGeometry(const QVector<RhiMesh*>& meshes) {
    m_capVerts.clear();

    QVector3D normal(m_clipPlane.x(), m_clipPlane.y(), m_clipPlane.z());
    const float length = normal.length();
    if (length < 1e-8f) {
        m_capVertexCount = 0;
        return;
    }
    normal /= length;
    const float planeD = m_clipPlane.w() / length;

    QVector3D u = QVector3D::crossProduct(
        normal, qAbs(normal.y()) > 0.9f ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0));
    if (u.lengthSquared() < 1e-8f) {
        u = QVector3D::crossProduct(normal, QVector3D(0, 0, 1));
    }
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(normal, u).normalized();

    for (const RhiMesh* mesh : meshes) {
        if (!mesh || !mesh->isVisible()) {
            continue;
        }
        const CpuMesh& cpu = mesh->cpuMesh();
        QVector<QVector3D> points;

        auto addEdge = [&points, &normal, planeD](const QVector3D& a,
                                                  const QVector3D& b,
                                                  float da, float db) {
            if ((da > 0.0f) == (db > 0.0f)) {
                return;
            }
            const float denom = da - db;
            if (qAbs(denom) < 1e-8f) {
                return;
            }
            const float t = da / denom;
            points.append(a + (b - a) * t);
        };

        for (int i = 0; i + 2 < cpu.indices.size(); i += 3) {
            const QVector3D& a = cpu.vertices[cpu.indices[i]].position;
            const QVector3D& b = cpu.vertices[cpu.indices[i + 1]].position;
            const QVector3D& c = cpu.vertices[cpu.indices[i + 2]].position;
            const float da = QVector3D::dotProduct(normal, a) + planeD;
            const float db = QVector3D::dotProduct(normal, b) + planeD;
            const float dc = QVector3D::dotProduct(normal, c) + planeD;

            addEdge(a, b, da, db);
            addEdge(b, c, db, dc);
            addEdge(c, a, dc, da);
        }

        if (points.size() < 3) {
            continue;
        }

        QVector3D centroid;
        for (const QVector3D& p : points) {
            centroid += p;
        }
        centroid /= float(points.size());

        std::sort(points.begin(), points.end(),
                  [&centroid, &u, &v](const QVector3D& a, const QVector3D& b) {
                      const QVector3D da = a - centroid;
                      const QVector3D db = b - centroid;
                      return qAtan2(QVector3D::dotProduct(da, v),
                                    QVector3D::dotProduct(da, u)) <
                             qAtan2(QVector3D::dotProduct(db, v),
                                    QVector3D::dotProduct(db, u));
                  });

        for (int i = 0; i < points.size(); ++i) {
            m_capVerts.append(centroid);
            m_capVerts.append(points[i]);
            m_capVerts.append(points[(i + 1) % points.size()]);
        }
    }

    m_capVertexCount = m_capVerts.size();
}

void RhiRenderer::uploadCapGeometry(QRhiResourceUpdateBatch* batch) {
    delete m_capVBuf;
    m_capVBuf = nullptr;

    if (m_capVertexCount > 0) {
        m_capVBuf = m_rhi->newBuffer(
            QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
            quint32(m_capVertexCount * sizeof(QVector3D)));
        m_capVBuf->create();
        batch->uploadStaticBuffer(m_capVBuf, m_capVerts.constData());
    }
    m_capDirty = false;
}

void RhiRenderer::drawCap(QRhiCommandBuffer* cb, int slot) {
    if (!m_capPipeline || !m_capVBuf || m_capVertexCount <= 0) {
        return;
    }
    cb->setGraphicsPipeline(m_capPipeline);
    const auto offset = frameOffset(slot);
    cb->setShaderResources(m_capSrb, 1, &offset);
    const QRhiCommandBuffer::VertexInput binding(m_capVBuf, 0);
    cb->setVertexInput(0, 1, &binding);
    cb->draw(m_capVertexCount);
}

void RhiRenderer::drawAxisLabels(QRhiCommandBuffer* cb, int slot) {
    if (!m_axisVisible || !m_labelPipeline || !m_labelVBuf ||
        m_labelVertexCount <= 0) {
        return;
    }
    cb->setGraphicsPipeline(m_labelPipeline);
    const auto offset = frameOffset(slot);
    cb->setShaderResources(m_labelSrb, 1, &offset);
    const QRhiCommandBuffer::VertexInput binding(m_labelVBuf, 0);
    cb->setVertexInput(0, 1, &binding);
    cb->draw(m_labelVertexCount);
}

void RhiRenderer::beginSlot(int slot) {
    m_currentSlot = qBound(0, slot, 3);
    m_frustum = Frustum::fromMatrix(m_slotViewProj[m_currentSlot]);
    m_frustumValid = m_cullEnabled;
}

bool RhiRenderer::isMeshCulled(const RhiMesh* mesh) const {
    if (!mesh || !mesh->isVisible()) {
        return true;
    }
    if (!m_frustumValid) {
        return false;
    }
    return !m_frustum.intersectsSphere(mesh->boundingCenter(),
                                       mesh->boundingRadius());
}

int RhiRenderer::lodLevelFor(const RhiMesh* mesh) const {
    if (!m_lodEnabled || !mesh) {
        return 0;
    }

    // 投影半径（相对屏幕高度）：radius / (dist * tan(fov/2))
    const float dist = qMax(
        (mesh->boundingCenter() - m_slotEye[m_currentSlot]).length(), 1e-4f);
    const float scale = qMax(m_slotProjScale[m_currentSlot], 1e-4f);
    const float projected = mesh->boundingRadius() / dist / scale;

    if (projected < 0.05f && mesh->lodIndexCount(2) > 0) {
        return 2;
    }
    if (projected < 0.14f && mesh->lodIndexCount(1) > 0) {
        return 1;
    }
    return 0;
}

void RhiRenderer::drawMeshesIndexed(QRhiCommandBuffer* cb,
                                    const QVector<RhiMesh*>& meshes,
                                    QRhiShaderResourceBindings* srb, int slot,
                                    bool useMaterial) {
    if (!srb) {
        return;
    }

    beginSlot(slot);

    if (!useMaterial) {
        const auto offset = frameOffset(slot);
        cb->setShaderResources(srb, 1, &offset);
    }

    int lastMaterialSlot = -1;

    for (int i = 0; i < meshes.size(); ++i) {
        RhiMesh* mesh = meshes[i];
        if (isMeshCulled(mesh) || !mesh->vertexBuffer() ||
            !mesh->indexBuffer()) {
            ++m_stats.culled;
            continue;
        }

        if (useMaterial) {
            const int materialSlot = m_meshMaterialSlot.value(i, i);
            if (materialSlot != lastMaterialSlot) {
                const QRhiCommandBuffer::DynamicOffset offsets[2] = {
                    frameOffset(slot), materialOffset(materialSlot)};
                cb->setShaderResources(srb, 2, offsets);
                lastMaterialSlot = materialSlot;
                ++m_stats.materialBatches;
            }
        }

        const int lod = lodLevelFor(mesh);
        QRhiBuffer* indexBuffer = mesh->indexBuffer();
        int indexCount = mesh->indexCount();
        if (lod > 0 && mesh->lodIndexBuffer(lod)) {
            indexBuffer = mesh->lodIndexBuffer(lod);
            indexCount = mesh->lodIndexCount(lod);
        }

        if (indexCount <= 0) {
            continue;
        }

        const QRhiCommandBuffer::VertexInput binding(mesh->vertexBuffer(), 0);
        cb->setVertexInput(0, 1, &binding, indexBuffer, 0,
                           QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(indexCount);

        ++m_stats.drawn;
        ++m_stats.drawCalls;
        m_stats.triangles += indexCount / 3;
        if (lod == 0) {
            ++m_stats.lod0;
        } else if (lod == 1) {
            ++m_stats.lod1;
        } else {
            ++m_stats.lod2;
        }
    }
}

void RhiRenderer::drawWireframe(QRhiCommandBuffer* cb,
                                const QVector<RhiMesh*>& meshes,
                                QRhiShaderResourceBindings* srb, int slot) {
    if (!srb) {
        return;
    }
    beginSlot(slot);
    const auto offset = frameOffset(slot);
    cb->setShaderResources(srb, 1, &offset);

    for (RhiMesh* mesh : meshes) {
        if (isMeshCulled(mesh) || !mesh->vertexBuffer() ||
            !mesh->lineIndexBuffer()) {
            ++m_stats.culled;
            continue;
        }
        const QRhiCommandBuffer::VertexInput binding(mesh->vertexBuffer(), 0);
        cb->setVertexInput(0, 1, &binding, mesh->lineIndexBuffer(), 0,
                           QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(mesh->lineIndexCount());
        ++m_stats.drawn;
        ++m_stats.drawCalls;
    }
}

void RhiRenderer::drawNormalLines(QRhiCommandBuffer* cb,
                                  const QVector<RhiMesh*>& meshes,
                                  bool faceNormals,
                                  QRhiShaderResourceBindings* srb, int slot) {
    if (!srb) {
        return;
    }
    beginSlot(slot);
    const auto offset = frameOffset(slot);
    cb->setShaderResources(srb, 1, &offset);

    for (RhiMesh* mesh : meshes) {
        if (isMeshCulled(mesh)) {
            continue;
        }
        QRhiBuffer* buffer = faceNormals ? mesh->faceNormalBuffer()
                                         : mesh->vertexNormalBuffer();
        const int count = faceNormals ? mesh->faceNormalVertexCount()
                                      : mesh->vertexNormalVertexCount();
        if (!buffer || count <= 0) {
            continue;
        }
        const QRhiCommandBuffer::VertexInput binding(buffer, 0);
        cb->setVertexInput(0, 1, &binding);
        cb->draw(count);
        ++m_stats.drawCalls;
    }
}

void RhiRenderer::drawBackground(QRhiCommandBuffer* cb) {
    if (!m_bgPipeline || !m_bgVBuf) {
        return;
    }
    cb->setGraphicsPipeline(m_bgPipeline);
    cb->setShaderResources(m_bgSrb);
    const QRhiCommandBuffer::VertexInput binding(m_bgVBuf, 0);
    cb->setVertexInput(0, 1, &binding);
    cb->draw(6);
}

void RhiRenderer::drawSkybox(QRhiCommandBuffer* cb) {
    if (!m_skyboxPipeline || !m_skyboxVBuf) {
        return;
    }
    cb->setGraphicsPipeline(m_skyboxPipeline);
    cb->setShaderResources(m_skyboxSrb);
    const QRhiCommandBuffer::VertexInput binding(m_skyboxVBuf, 0);
    cb->setVertexInput(0, 1, &binding);
    cb->draw(36);
}

void RhiRenderer::drawSurface(QRhiCommandBuffer* cb,
                              const QVector<RhiMesh*>& meshes, int slot) {
    const bool usePbr = (m_displayStyle == DisplayStyle::Realistic) && m_pbrPipeline;
    QRhiGraphicsPipeline* pipeline = usePbr ? m_pbrPipeline : m_phongPipeline;
    QRhiShaderResourceBindings* srb = usePbr ? m_pbrSrb : m_phongSrb;
    if (!pipeline || !srb) {
        return;
    }
    cb->setGraphicsPipeline(pipeline);
    drawMeshesIndexed(cb, meshes, srb, slot, true);
}

void RhiRenderer::drawWireOverlay(QRhiCommandBuffer* cb,
                                  const QVector<RhiMesh*>& meshes, int slot) {
    if (!m_wireOverlayPipeline || !m_wireframeSrb) {
        return;
    }
    cb->setGraphicsPipeline(m_wireOverlayPipeline);
    drawWireframe(cb, meshes, m_wireframeSrb, slot);
}

FrameUBOData RhiRenderer::makeFrameData(const Camera& cam, const Light& light,
                                        float aspect, const QMatrix4x4& lightSpace,
                                        const QMatrix4x4& shadowBias) const {
    FrameUBOData data{};

    const QMatrix4x4 model;
    const QMatrix4x4 view = cam.viewMatrix();
    const QMatrix4x4 proj =
        m_rhi->clipSpaceCorrMatrix() * cam.projectionMatrix(aspect);
    const QMatrix4x4 normalMat = (view * model).inverted().transposed();

    copyMatrix(data.model, model);
    copyMatrix(data.view, view);
    copyMatrix(data.projection, proj);
    copyMatrix(data.normalMatrix, normalMat);
    copyMatrix(data.lightSpace, lightSpace);
    copyMatrix(data.shadowBias, shadowBias);

    data.lightPos[0] = light.position.x();
    data.lightPos[1] = light.position.y();
    data.lightPos[2] = light.position.z();
    data.lightColor[0] = light.color.x();
    data.lightColor[1] = light.color.y();
    data.lightColor[2] = light.color.z();
    data.viewPos[0] = cam.position().x();
    data.viewPos[1] = cam.position().y();
    data.viewPos[2] = cam.position().z();

    return data;
}

void RhiRenderer::renderShadowPass(QRhiCommandBuffer* cb,
                                   const QVector<RhiMesh*>& meshes,
                                   const QMatrix4x4& lightSpace,
                                   const QMatrix4x4& shadowBias) {
    if (!m_shadowPipeline || !m_shadowRt) {
        return;
    }

    ShadowUBOData shadowData{};
    copyMatrix(shadowData.lightSpace, lightSpace);
    copyMatrix(shadowData.shadowBias, shadowBias);

    auto* batch = m_rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(m_shadowUBO, 0, sizeof(shadowData), &shadowData);

    cb->beginPass(m_shadowRt, QColor(), QRhiDepthStencilClearValue(1.0f, 0), batch);
    cb->setViewport({0, 0, 2048.0f, 2048.0f});
    cb->setGraphicsPipeline(m_shadowPipeline);
    cb->setShaderResources(m_shadowSrb);

    for (RhiMesh* mesh : meshes) {
        if (!mesh || !mesh->isVisible() || !mesh->vertexBuffer() || !mesh->indexBuffer()) {
            continue;
        }
        const QRhiCommandBuffer::VertexInput binding(mesh->vertexBuffer(), 0);
        cb->setVertexInput(0, 1, &binding, mesh->indexBuffer(), 0,
                           QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(mesh->indexCount());
    }

    cb->endPass();
}

void RhiRenderer::drawMultiView(QRhiCommandBuffer* cb,
                                const QVector<RhiMesh*>& meshes,
                                float width, float height) {
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;

    for (int i = 0; i < 4; ++i) {
        const float x = (i % 2) * halfWidth;
        const float y = (i / 2) * halfHeight;
        cb->setViewport({x, y, halfWidth, halfHeight});
        cb->setScissor({int(x), int(y), int(halfWidth), int(halfHeight)});
        beginSlot(i);

        if (m_displayStyle == DisplayStyle::Wireframe) {
            if (m_wireframePipeline) {
                cb->setGraphicsPipeline(m_wireframePipeline);
                drawWireframe(cb, meshes, m_wireframeSrb, i);
            }
        } else {
            drawSurface(cb, meshes, i);
            if (m_displayStyle == DisplayStyle::WireShaded) {
                drawWireOverlay(cb, meshes, i);
            }
        }
        drawAxisGrid(cb, i);
        drawAxisLabels(cb, i);
    }

    cb->setViewport({0, 0, width, height});
    cb->setScissor({0, 0, int(width), int(height)});
}

void RhiRenderer::render(QRhiCommandBuffer* cb,
                         const QVector<RhiMesh*>& meshes,
                         const Camera& cam,
                         const Light& light,
                         const Material& mat) {
    if (!m_initialized || !m_rt) {
        return;
    }

    uploadMeshes(cb, meshes);

    m_bufferPool.beginFrame();
    m_stats = RenderStats();
    m_stats.objects = meshes.size();
    m_stats.pooledBuffers = m_bufferPool.cachedCount();

    (void)mat;   // 材质已按对象存储

    if (m_diffuseDirty && m_initialized) {
        destroyPipelines();
        createDiffuseTexture();
        buildAllPipelines();
        m_diffuseDirty = false;
    }

    const QSize size = m_rt->pixelSize();
    const float width = qMax(1.0f, float(size.width()));
    const float height = qMax(1.0f, float(size.height()));

    // 后处理开启时，尺寸变化需要同步重建离屏目标与场景管线
    if (isPostEnabled() && (!m_sceneRt || m_sceneSize != size)) {
        destroyPipelines();
        ensureSceneTarget(size);
        m_pipelineRp = m_sceneRp;
        buildAllPipelines();
    }

    const bool usePost = isPostEnabled() && m_sceneRt && m_postPipeline;
    QRhiRenderTarget* sceneTarget = usePost ? m_sceneRt : m_rt;

    const bool splitScreen = (m_renderMode == RenderMode::SplitScreen);
    const float aspect = splitScreen ? (width * 0.5f) / height : width / height;

    // ── 相机集合（多视图时 4 个）────────────────────────────
    const int slotCount = m_multiView ? 4 : 1;
    Camera viewCameras[4];
    int presets[4] = {6, 0, 4, 3};   // 等轴测 / 前 / 顶 / 右

    BoundingBox sceneBox;
    sceneBox.expand(m_sceneCenter - QVector3D(m_sceneRadius, m_sceneRadius, m_sceneRadius));
    sceneBox.expand(m_sceneCenter + QVector3D(m_sceneRadius, m_sceneRadius, m_sceneRadius));

    for (int i = 0; i < slotCount; ++i) {
        viewCameras[i].setAspectRatio(aspect);
        viewCameras[i].setProjectionType(cam.projectionType());
        if (m_multiView) {
            viewCameras[i].setViewPreset(presets[i], sceneBox);
        } else {
            viewCameras[i].setTarget(cam.target());
            viewCameras[i].setPosition(cam.position());
            viewCameras[i].setUp(cam.upDirection());
        }
    }

    // 记录各视口的相机状态，供视锥剔除与距离 LOD 使用
    for (int i = 0; i < 4; ++i) {
        const Camera& camera = viewCameras[i < slotCount ? i : 0];
        const QMatrix4x4 view = camera.viewMatrix();
        const QMatrix4x4 proj = m_rhi->clipSpaceCorrMatrix() *
                                camera.projectionMatrix(aspect);
        m_slotViewProj[i] = proj * view;
        m_slotEye[i] = camera.position();
        const float p11 = proj(1, 1);
        m_slotProjScale[i] = (qAbs(p11) > 1e-6f) ? 1.0f / qAbs(p11) : 0.4f;
    }

    // ── 阴影矩阵 ────────────────────────────────────────────
    QMatrix4x4 lightSpace;
    QMatrix4x4 shadowBias;
    if (m_renderMode == RenderMode::ShadowMapping && !m_multiView &&
        m_shadowedPhongPipeline) {
        QVector3D lightDir = light.position;
        if (lightDir.lengthSquared() < 1e-8f) {
            lightDir = QVector3D(0, 1, 0);
        }
        lightDir.normalize();

        const float radius = qMax(m_sceneRadius, 1.0f);
        const QVector3D lightEye = m_sceneCenter + lightDir * radius * 3.0f;
        const QVector3D up =
            (qAbs(lightDir.y()) > 0.99f) ? QVector3D(0, 0, 1) : QVector3D(0, 1, 0);

        QMatrix4x4 lightView;
        lightView.lookAt(lightEye, m_sceneCenter, up);
        QMatrix4x4 lightProj;
        lightProj.ortho(-radius * 1.6f, radius * 1.6f,
                        -radius * 1.6f, radius * 1.6f,
                        0.01f, radius * 8.0f);
        lightSpace = m_rhi->clipSpaceCorrMatrix() * lightProj * lightView;

        shadowBias.setToIdentity();
        shadowBias(0, 0) = 0.5f;
        shadowBias(0, 3) = 0.5f;
        if (m_rhi->isClipDepthZeroToOne()) {
            shadowBias(1, 1) = m_rhi->isYUpInNDC() ? -0.5f : 0.5f;
            shadowBias(1, 3) = 0.5f;
        } else {
            shadowBias(1, 1) = 0.5f;
            shadowBias(1, 3) = 0.5f;
            shadowBias(2, 2) = 0.5f;
            shadowBias(2, 3) = 0.5f;
        }

        renderShadowPass(cb, meshes, lightSpace, shadowBias);
    }

    // ── 材质/背景/法线/裁剪/选择 UBO ────────────────────────
    BackgroundUBOData bgData{};
    bgData.topColor[0] = m_bgTopColor.x();
    bgData.topColor[1] = m_bgTopColor.y();
    bgData.topColor[2] = m_bgTopColor.z();
    bgData.topColor[3] = 1.0f;
    bgData.botColor[0] = m_bgBotColor.x();
    bgData.botColor[1] = m_bgBotColor.y();
    bgData.botColor[2] = m_bgBotColor.z();
    bgData.botColor[3] = 1.0f;

    NormalUBOData normalData{};
    normalData.length = m_normalLength;
    if (m_renderMode == RenderMode::FaceNormals) {
        normalData.color[0] = 1.0f;
        normalData.color[1] = 0.85f;
        normalData.color[2] = 0.2f;
    } else {
        normalData.color[0] = 0.2f;
        normalData.color[1] = 0.9f;
        normalData.color[2] = 1.0f;
    }

    ClipUBOData clipData{};
    clipData.plane[0] = m_clipPlane.x();
    clipData.plane[1] = m_clipPlane.y();
    clipData.plane[2] = m_clipPlane.z();
    clipData.plane[3] = m_clipPlane.w();

    SelectionUBOData selectionData{};
    selectionData.color[0] = 1.0f;
    selectionData.color[1] = 0.55f;
    selectionData.color[2] = 0.1f;
    selectionData.color[3] = 1.0f;

    // ── 主 Pass 资源更新 ────────────────────────────────────
    auto* updates = m_rhi->nextResourceUpdateBatch();

    for (int i = 0; i < slotCount; ++i) {
        const FrameUBOData frame = makeFrameData(viewCameras[i], light, aspect,
                                                 lightSpace, shadowBias);
        updates->updateDynamicBuffer(m_frameUBO, quint32(i * m_frameStride),
                                     sizeof(FrameUBOData), &frame);
    }

    // 逐对象材质：相同材质复用同一槽位，绘制时可合并状态切换（批处理）
    m_meshMaterialSlot.resize(meshes.size());
    m_meshMaterialSlot.fill(-1);

    QHash<quint64, int> materialSlotMap;
    QVector<MaterialUBOData> materialSlots;
    for (int i = 0; i < meshes.size(); ++i) {
        if (!meshes[i]) {
            continue;
        }
        const Material& meshMaterial = meshes[i]->material();
        const float values[12] = {
            meshMaterial.ambient.x(), meshMaterial.ambient.y(),
            meshMaterial.ambient.z(), meshMaterial.diffuse.x(),
            meshMaterial.diffuse.y(), meshMaterial.diffuse.z(),
            meshMaterial.specular.x(), meshMaterial.specular.y(),
            meshMaterial.specular.z(), meshMaterial.shininess,
            meshMaterial.metallic, meshMaterial.roughness};
        const quint64 hash =
            qHash(QByteArray(reinterpret_cast<const char*>(values),
                             sizeof(values)));

        auto it = materialSlotMap.find(hash);
        if (it != materialSlotMap.end()) {
            m_meshMaterialSlot[i] = it.value();
            continue;
        }

        if (materialSlots.size() >= m_materialSlots) {
            m_meshMaterialSlot[i] = m_materialSlots - 1;
            continue;
        }

        MaterialUBOData data{};
        data.shininess = meshMaterial.shininess;
        data.metallic = meshMaterial.metallic;
        data.roughness = meshMaterial.roughness;
        std::memcpy(data.ambient, &meshMaterial.ambient, sizeof(float) * 3);
        std::memcpy(data.diffuse, &meshMaterial.diffuse, sizeof(float) * 3);
        std::memcpy(data.specular, &meshMaterial.specular, sizeof(float) * 3);

        const int slot = materialSlots.size();
        materialSlots.append(data);
        materialSlotMap.insert(hash, slot);
        m_meshMaterialSlot[i] = slot;
    }

    for (int i = 0; i < materialSlots.size(); ++i) {
        updates->updateDynamicBuffer(m_materialUBO, quint32(i * m_materialStride),
                                     sizeof(MaterialUBOData), &materialSlots[i]);
    }

    if (m_diffuseUploadPending && m_diffuseTexture) {
        QImage image = m_diffuseImage;
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_RGBA8888);
            image.fill(Qt::white);
        } else {
            image = image.convertToFormat(QImage::Format_RGBA8888);
        }
        updates->uploadTexture(m_diffuseTexture, image);
        m_diffuseUploadPending = false;
    }

    updates->updateDynamicBuffer(m_bgUBO, 0, sizeof(bgData), &bgData);
    updates->updateDynamicBuffer(m_normalUBO, 0, sizeof(normalData), &normalData);
    updates->updateDynamicBuffer(m_clipUBO, 0, sizeof(clipData), &clipData);
    updates->updateDynamicBuffer(m_selectionUBO, 0, sizeof(selectionData), &selectionData);

    if (m_renderMode == RenderMode::Skybox || m_multiView) {
        QMatrix4x4 skyView = viewCameras[0].viewMatrix();
        skyView.setColumn(3, QVector4D(0, 0, 0, 1));
        SkyboxUBOData skyData{};
        copyMatrix(skyData.projection,
                   m_rhi->clipSpaceCorrMatrix() *
                       viewCameras[0].projectionMatrix(aspect));
        copyMatrix(skyData.view, skyView);
        updates->updateDynamicBuffer(m_skyboxUBO, 0, sizeof(skyData), &skyData);
    }

    if (m_axisDirty) {
        uploadAxisGrid(updates);
    }

    if (m_renderMode == RenderMode::ClippingPlane && m_capDirty) {
        rebuildCapGeometry(meshes);
        uploadCapGeometry(updates);
    }

    auto finishPost = [&]() {
        if (!usePost) {
            return;
        }

        PostUBOData postData{};
        postData.texelX = 1.0f / width;
        postData.texelY = 1.0f / height;
        postData.flipY = m_rhi->isYUpInNDC() ? 1.0f : 0.0f;
        postData.aoStrength = m_aoStrength;
        postData.nearPlane = 0.01f;
        postData.farPlane = 1000.0f;
        postData.aoRadius = 2.5f;
        postData.fxaaEnabled = m_fxaaEnabled ? 1.0f : 0.0f;
        postData.aoEnabled = m_aoEnabled ? 1.0f : 0.0f;
        postData.exposure = 1.0f;

        auto* postBatch = m_rhi->nextResourceUpdateBatch();
        postBatch->updateDynamicBuffer(m_postUBO, 0, sizeof(postData), &postData);

        cb->beginPass(m_rt, QColor(0, 0, 0), QRhiDepthStencilClearValue(1.0f, 0),
                      postBatch);
        cb->setViewport({0, 0, width, height});
        cb->setScissor({0, 0, int(width), int(height)});
        drawPost(cb);
        cb->endPass();
    };

    cb->beginPass(sceneTarget, QColor(30, 30, 30),
                  QRhiDepthStencilClearValue(1.0f, 0), updates);
    cb->setViewport({0, 0, width, height});

    if (m_multiView) {
        drawBackground(cb);
        drawMultiView(cb, meshes, width, height);
        cb->endPass();
        finishPost();
        return;
    }

    if (m_renderMode == RenderMode::Skybox) {
        drawSkybox(cb);
    } else {
        drawBackground(cb);
    }

    beginSlot(0);

    switch (m_renderMode) {
        case RenderMode::Phong:
            if (m_displayStyle == DisplayStyle::Wireframe) {
                if (m_wireframePipeline) {
                    cb->setGraphicsPipeline(m_wireframePipeline);
                    drawWireframe(cb, meshes, m_wireframeSrb, 0);
                }
            } else {
                drawSurface(cb, meshes, 0);
                if (m_displayStyle == DisplayStyle::WireShaded) {
                    drawWireOverlay(cb, meshes, 0);
                }
            }
            break;

        case RenderMode::Wireframe:
            if (m_wireframePipeline) {
                cb->setGraphicsPipeline(m_wireframePipeline);
                drawWireframe(cb, meshes, m_wireframeSrb, 0);
            }
            break;

        case RenderMode::Normals:
            if (m_normalsPipeline) {
                cb->setGraphicsPipeline(m_normalsPipeline);
                drawMeshesIndexed(cb, meshes, m_normalsSrb, 0);
            }
            break;

        case RenderMode::FaceNormals:
        case RenderMode::VertexNormals:
            drawSurface(cb, meshes, 0);
            if (m_displayStyle == DisplayStyle::WireShaded) {
                drawWireOverlay(cb, meshes, 0);
            }
            if (m_normalLinesPipeline) {
                cb->setGraphicsPipeline(m_normalLinesPipeline);
                drawNormalLines(cb, meshes, m_renderMode == RenderMode::FaceNormals,
                                m_normalLinesSrb, 0);
            }
            break;

        case RenderMode::FlatShading:
            if (m_flatPipeline) {
                cb->setGraphicsPipeline(m_flatPipeline);
                drawMeshesIndexed(cb, meshes, m_flatSrb, 0, true);
            }
            break;

        case RenderMode::PBR:
            if (m_pbrPipeline) {
                cb->setGraphicsPipeline(m_pbrPipeline);
                drawMeshesIndexed(cb, meshes, m_pbrSrb, 0, true);
            }
            break;

        case RenderMode::ShadowMapping:
            if (m_shadowedPhongPipeline) {
                cb->setGraphicsPipeline(m_shadowedPhongPipeline);
                drawMeshesIndexed(cb, meshes, m_shadowedPhongSrb, 0, true);
            } else {
                drawSurface(cb, meshes, 0);
            }
            break;

        case RenderMode::Skybox:
            drawSurface(cb, meshes, 0);
            if (m_displayStyle == DisplayStyle::WireShaded) {
                drawWireOverlay(cb, meshes, 0);
            }
            break;

        case RenderMode::Selection:
            drawSurface(cb, meshes, 0);
            if (m_selectionPipeline && m_selectedMesh >= 0 &&
                m_selectedMesh < meshes.size() && meshes[m_selectedMesh] &&
                meshes[m_selectedMesh]->isVisible()) {
                cb->setGraphicsPipeline(m_selectionPipeline);
                const auto offset = frameOffset(0);
                cb->setShaderResources(m_selectionSrb, 1, &offset);
                RhiMesh* mesh = meshes[m_selectedMesh];
                if (mesh->vertexBuffer() && mesh->indexBuffer()) {
                    const QRhiCommandBuffer::VertexInput binding(mesh->vertexBuffer(), 0);
                    cb->setVertexInput(0, 1, &binding, mesh->indexBuffer(), 0,
                                       QRhiCommandBuffer::IndexUInt32);
                    cb->drawIndexed(mesh->indexCount());
                }
            }
            break;

        case RenderMode::ClippingPlane:
            if (m_clippedPipeline) {
                cb->setGraphicsPipeline(m_clippedPipeline);
                drawMeshesIndexed(cb, meshes, m_clippedSrb, 0, true);
            }
            drawCap(cb, 0);
            break;

        case RenderMode::SplitScreen: {
            const float halfWidth = width * 0.5f;
            cb->setViewport({0, 0, halfWidth, height});
            cb->setScissor({0, 0, int(halfWidth), int(height)});
            drawSurface(cb, meshes, 0);

            if (m_wireframePipeline) {
                cb->setViewport({halfWidth, 0, halfWidth, height});
                cb->setScissor({int(halfWidth), 0, int(halfWidth), int(height)});
                cb->setGraphicsPipeline(m_wireframePipeline);
                drawWireframe(cb, meshes, m_wireframeSrb, 0);
            }
            cb->setViewport({0, 0, width, height});
            cb->setScissor({0, 0, int(width), int(height)});
            break;
        }
    }

    drawAxisGrid(cb, 0);
    drawAxisLabels(cb, 0);

    cb->endPass();
    finishPost();
}
