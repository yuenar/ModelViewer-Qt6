#include "RhiWidget.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../math/BoundingBox.h"
#include "../math/Camera.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFileDialog>

RhiWidget::RhiWidget(QWidget* parent)
    : QRhiWidget(parent)
{
}

RhiWidget::~RhiWidget() {
    // 在 RHI 上下文仍然有效时释放渲染器资源
    // 这确保所有 GPU 资源被正确释放，避免警告
    if (m_renderer && rhi()) {
        delete m_renderer;
        m_renderer = nullptr;
    }
    
    // 清理网格资源
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

void RhiWidget::setBackgroundColors(const QVector3D& topColor, const QVector3D& botColor) {
    if (m_renderer) {
        m_renderer->setBackgroundColors(topColor, botColor);
        update();
    }
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

void RhiWidget::keyPressEvent(QKeyEvent* e) {
    if (e->isAutoRepeat()) {
        e->ignore();
        return;
    }
    
    switch (e->key()) {
        case Qt::Key_F:
            // F：聚焦视图
            fitToView();
            e->accept();
            break;
            
        case Qt::Key_Space:
            // Space：重置视图
            m_camera.reset();
            update();
            e->accept();
            break;
            
        case Qt::Key_W:
            // W：切换线框模式
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 1) ? 0 : 1;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_N:
            // N：显示法线
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 2) ? 0 : 2;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_L:
            // L：切换光照（在 Phong 和 Wireframe 之间切换）
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 0) ? 1 : 0;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_S:
            // S：切换阴影（切换到阴影映射模式）
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 7) ? 0 : 7;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_P:
            // P：切换投影
            toggleProjection();
            e->accept();
            break;
            
        case Qt::Key_F11:
            // F11：全屏模式
            if (window()->isFullScreen()) {
                window()->showNormal();
            } else {
                window()->showFullScreen();
            }
            e->accept();
            break;
            
        default:
            QRhiWidget::keyPressEvent(e);
            break;
    }
}


bool RhiWidget::saveScreenshotEx(const QString& filePath, const QString& format, int quality) {
    QImage image = grabFramebuffer();
    if (image.isNull()) {
        qWarning() << "Failed to grab framebuffer";
        return false;
    }
    
    bool success = false;
    if (format.toLower() == "jpg" || format.toLower() == "jpeg") {
        success = image.save(filePath, "JPEG", quality);
    } else {
        success = image.save(filePath, format.toUpper().toStdString().c_str());
    }
    
    if (!success) {
        qWarning() << "Failed to save screenshot to" << filePath;
    } else {
        qDebug() << "Screenshot saved to" << filePath << "with format" << format;
    }
    
    return success;
}


QString RhiWidget::getModelStatistics() const {
    if (m_meshes.isEmpty()) {
        return "No model loaded";
    }
    
    uint32_t totalVertices = 0;
    uint32_t totalIndices = 0;
    
    for (const auto* mesh : m_meshes) {
        if (mesh) {
            totalVertices += mesh->vertexCount();
            totalIndices += mesh->indexCount();
        }
    }
    
    uint32_t triangles = totalIndices / 3;
    
    QVector3D minPt = m_sceneBbox.min();
    QVector3D maxPt = m_sceneBbox.max();
    QVector3D sizePt = maxPt - minPt;
    
    QString stats = QString(
        "Model Statistics:\n"
        "Meshes: %1\n"
        "Vertices: %2\n"
        "Triangles: %3\n"
        "Bounding Box:\n"
        "  Min: (%4, %5, %6)\n"
        "  Max: (%7, %8, %9)\n"
        "  Size: (%10, %11, %12)"
    ).arg(m_meshes.size())
     .arg(totalVertices)
     .arg(triangles)
     .arg(QString::number(minPt.x(), 'f', 2))
     .arg(QString::number(minPt.y(), 'f', 2))
     .arg(QString::number(minPt.z(), 'f', 2))
     .arg(QString::number(maxPt.x(), 'f', 2))
     .arg(QString::number(maxPt.y(), 'f', 2))
     .arg(QString::number(maxPt.z(), 'f', 2))
     .arg(QString::number(sizePt.x(), 'f', 2))
     .arg(QString::number(sizePt.y(), 'f', 2))
     .arg(QString::number(sizePt.z(), 'f', 2));
    
    return stats;
}
