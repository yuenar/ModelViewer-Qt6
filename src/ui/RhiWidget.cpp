#include "RhiWidget.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../math/BoundingBox.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileDialog>

RhiWidget::RhiWidget(QWidget* parent)
    : QRhiWidget(parent)
{
}

RhiWidget::~RhiWidget() {
    delete m_renderer;
    qDeleteAll(m_meshes);
}

void RhiWidget::initialize(QRhiCommandBuffer* cb) {
    if (!m_renderer) {
        m_renderer = new RhiRenderer(rhi(), renderTarget());
        m_renderer->initialize(cb);
    } else {
        m_renderer->setRenderTarget(renderTarget());
        m_renderer->releaseAndRebuildPipelines();
    }
}

void RhiWidget::render(QRhiCommandBuffer* cb) {
    const QSize pixelSize = renderTarget()->pixelSize();
    m_camera.setAspectRatio(float(pixelSize.width()) / float(pixelSize.height()));
    
    m_renderer->render(cb, m_meshes, m_camera, m_light, m_material);
}

void RhiWidget::loadModel(const QString& filePath) {
    qDeleteAll(m_meshes);
    m_meshes.clear();
    
    ModelLoader loader;
    const auto cpuMeshes = loader.loadCPU(filePath);
    
    BoundingBox sceneBbox;
    for (auto& cpuMesh : cpuMeshes) {
        auto* mesh = new RhiMesh(rhi(), cpuMesh);
        mesh->upload();
        m_meshes.append(mesh);
        sceneBbox.expand(cpuMesh.bbox);
    }
    m_camera.fitToView(sceneBbox);
    update();
}

void RhiWidget::setRenderMode(int mode) {
    if (m_renderer) {
        m_renderer->setRenderMode(mode);
        update();
    }
}

void RhiWidget::setMaterial(const Material& mat) {
    m_material = mat;
    update();
}

void RhiWidget::setLight(const Light& light) {
    m_light = light;
    update();
}

void RhiWidget::fitToView() {
    if (!m_meshes.isEmpty()) {
        BoundingBox sceneBbox;
        // 需要从CpuMesh获取bbox，暂时使用默认值
        // 实际应该在加载时保存bbox信息
        m_camera.fitToView(sceneBbox);
        update();
    }
}

void RhiWidget::mousePressEvent(QMouseEvent* e) {
    m_lastMousePos = e->pos();
    m_activeButton = e->button();
    if (e->button() == Qt::LeftButton)
        m_camera.beginRotate(e->pos());
}

void RhiWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_activeButton == Qt::LeftButton) {
        m_camera.rotate(m_lastMousePos, e->pos(), size());
    } else if (m_activeButton == Qt::MiddleButton) {
        const QPoint delta = e->pos() - m_lastMousePos;
        m_camera.pan(delta);
    }
    m_lastMousePos = e->pos();
    update();
}

void RhiWidget::mouseReleaseEvent(QMouseEvent*) {
    m_activeButton = Qt::NoButton;
}

void RhiWidget::wheelEvent(QWheelEvent* e) {
    m_camera.zoom(e->angleDelta().y() / 120.0f);
    update();
}
