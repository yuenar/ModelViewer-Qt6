#include "RhiWidget.h"
#include "../renderer/RhiRenderer.h"
#include "../loader/ModelLoader.h"
#include "../math/BoundingBox.h"
#include "../math/Camera.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QtMath>

RhiWidget::RhiWidget(QWidget* parent)
    : QRhiWidget(parent)
{
    // 设置焦点策略，使 RhiWidget 能够接收键盘事件
    setFocusPolicy(Qt::StrongFocus);
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
    qDebug() << "keyPressEvent received, key:" << e->key() << "text:" << e->text();
    
    if (e->isAutoRepeat()) {
        e->ignore();
        return;
    }
    
    switch (e->key()) {
        case Qt::Key_F:
            // F：聚焦视图
            qDebug() << "F key pressed - fitToView";
            fitToView();
            e->accept();
            break;
            
        case Qt::Key_Space:
            // Space：重置视图
            qDebug() << "Space key pressed - reset camera";
            m_camera.reset();
            update();
            e->accept();
            break;
            
        case Qt::Key_W:
            // W：切换线框模式
            qDebug() << "W key pressed - toggle wireframe";
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 1) ? 0 : 1;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_N:
            // N：显示法线
            qDebug() << "N key pressed - toggle normals";
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 2) ? 0 : 2;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_L:
            // L：切换光照（在 Phong 和 Wireframe 之间切换）
            qDebug() << "L key pressed - toggle lighting";
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 0) ? 1 : 0;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_S:
            // S：切换阴影（切换到阴影映射模式）
            qDebug() << "S key pressed - toggle shadow mapping";
            if (m_renderer) {
                static int mode = 0;
                mode = (mode == 7) ? 0 : 7;
                setRenderMode(mode);
            }
            e->accept();
            break;
            
        case Qt::Key_P:
            // P：切换投影
            qDebug() << "P key pressed - toggle projection";
            toggleProjection();
            e->accept();
            break;
            
        case Qt::Key_F11:
            // F11：全屏模式
            qDebug() << "F11 key pressed - toggle fullscreen";
            if (window()->isFullScreen()) {
                window()->showNormal();
            } else {
                window()->showFullScreen();
            }
            e->accept();
            break;
            
        case Qt::Key_1:
        case Qt::Key_2:
        case Qt::Key_3:
        case Qt::Key_4:
        case Qt::Key_5:
        case Qt::Key_6:
        case Qt::Key_7: {
            // 数字快捷键 1-7：视图预设
            int preset = e->key() - Qt::Key_1;
            qDebug() << "Number key pressed - view preset" << preset;
            
            if (m_meshes.isEmpty()) {
                qDebug() << "No model loaded, cannot apply view preset";
                e->accept();
                break;
            }
            
            // 获取模型的包围盒信息
            QVector3D center = m_sceneBbox.center();
            float radius = m_sceneBbox.radius();
            
            // 根据包围盒大小计算合适的距离
            float distance = radius / qSin(qDegreesToRadians(22.5f)); // 45度视场角的一半
            
            // 设置摄像机方向和目标
            QVector3D direction;
            QVector3D up(0, 1, 0);
            
            switch (preset) {
                case 0: // 前视图 - 沿 Z 轴正方向看
                    direction = QVector3D(0, 0, 1);
                    up = QVector3D(0, 1, 0);
                    break;
                case 1: // 后视图 - 沿 Z 轴负方向看
                    direction = QVector3D(0, 0, -1);
                    up = QVector3D(0, 1, 0);
                    break;
                case 2: // 左视图 - 沿 X 轴负方向看
                    direction = QVector3D(-1, 0, 0);
                    up = QVector3D(0, 1, 0);
                    break;
                case 3: // 右视图 - 沿 X 轴正方向看
                    direction = QVector3D(1, 0, 0);
                    up = QVector3D(0, 1, 0);
                    break;
                case 4: // 顶视图 - 从上往下看，沿 Y 轴负方向
                    direction = QVector3D(0, -1, 0);
                    up = QVector3D(1, 0, 0);  // 使用 X 轴作为上向量
                    break;
                case 5: // 底视图 - 从下往上看，沿 Y 轴正方向
                    direction = QVector3D(0, 1, 0);
                    up = QVector3D(1, 0, 0);  // 使用 X 轴作为上向量
                    break;
                case 6: // 等轴测视图
                    direction = QVector3D(1, 1, 1).normalized();
                    up = QVector3D(0, 1, 0);
                    break;
                default:
                    direction = QVector3D(0, 0, 1);
                    up = QVector3D(0, 1, 0);
            }
            
            // 设置摄像机位置：从目标点沿着方向的反方向移动距离
            m_camera.setTarget(center);
            m_camera.setPosition(center - direction * distance);
            m_camera.setUp(up);
            
            update();
            e->accept();
            break;
        }
            
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
     .arg(minPt.x(), 0, 'f', 2)
     .arg(minPt.y(), 0, 'f', 2)
     .arg(minPt.z(), 0, 'f', 2)
     .arg(maxPt.x(), 0, 'f', 2)
     .arg(maxPt.y(), 0, 'f', 2)
     .arg(maxPt.z(), 0, 'f', 2)
     .arg(sizePt.x(), 0, 'f', 2)
     .arg(sizePt.y(), 0, 'f', 2)
     .arg(sizePt.z(), 0, 'f', 2);
    
    return stats;
}
