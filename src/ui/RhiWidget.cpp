#include "RhiWidget.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../math/BoundingBox.h"
#include "../math/Camera.h"
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
    qDebug() << "RhiWidget::initialize called";
    if (!m_renderer) {
        m_renderer = new RhiRenderer(rhi(), renderTarget());
        m_renderer->initialize(cb);
    } else {
        m_renderer->setRenderTarget(renderTarget());
        m_renderer->releaseAndRebuildPipelines();
    }
}

void RhiWidget::render(QRhiCommandBuffer* cb) {
    qDebug() << "RhiWidget::render called, meshes:" << m_meshes.size();
    if (!m_renderer) {
        qDebug() << "ERROR: m_renderer is null!";
        return;
    }
    const QSize pixelSize = renderTarget()->pixelSize();
    qDebug() << "Render target size:" << pixelSize;
    m_camera.setAspectRatio(float(pixelSize.width()) / float(pixelSize.height()));
    
    qDebug() << "Calling m_renderer->render...";
    m_renderer->render(cb, m_meshes, m_camera, m_light, m_material);
    qDebug() << "m_renderer->render completed";
}

void RhiWidget::loadModel(const QString& filePath) {
    qDebug() << "loadModel called with path:" << filePath;
    
    if (filePath.isEmpty()) {
        qDebug() << "Empty path, not clearing meshes";
        return;
    }
    
    qDeleteAll(m_meshes);
    m_meshes.clear();
    m_sceneBbox = BoundingBox();
    
    ModelLoader loader;
    const auto cpuMeshes = loader.loadCPU(filePath);
    
    for (const auto& cpuMesh : cpuMeshes) {
        auto* mesh = new RhiMesh(nullptr, cpuMesh);
        m_meshes.append(mesh);
        m_sceneBbox.expand(cpuMesh.bbox);
    }
    m_camera.fitToView(m_sceneBbox);
    
    // 重置renderer的上传标志
    if (m_renderer) {
        m_renderer->resetMeshUploadFlag();
    }
    
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
        m_camera.fitToView(m_sceneBbox);
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

void RhiWidget::saveScreenshot(const QString& filePath) {
    QImage image = grabFramebuffer();
    if (image.isNull()) {
        qWarning() << "Failed to grab framebuffer";
        return;
    }
    
    if (!image.save(filePath)) {
        qWarning() << "Failed to save screenshot to" << filePath;
    } else {
        qDebug() << "Screenshot saved to" << filePath;
    }
}

void RhiWidget::toggleProjection() {
    // 切换投影类型
    static ProjectionType currentType = ProjectionType::Perspective;
    currentType = (currentType == ProjectionType::Perspective) 
                  ? ProjectionType::Orthographic 
                  : ProjectionType::Perspective;
    m_camera.setProjectionType(currentType);
    update();
}
