#pragma once
#include <QRhiWidget>
#include <QPoint>
#include "../math/Camera.h"
#include "../core/Material.h"
#include "../core/Light.h"
#include "../renderer/RhiMesh.h"

class RhiRenderer;

class RhiWidget : public QRhiWidget {
    Q_OBJECT
public:
    explicit RhiWidget(QWidget* parent = nullptr);
    ~RhiWidget() override;
    
    void loadModel(const QString& filePath);
    void setRenderMode(int mode);
    void setMaterial(const Material& mat);
    Material getMaterial() const { return m_material; }
    void setLight(const Light& light);
    Light getLight() const { return m_light; }
    void fitToView();
    void saveScreenshot(const QString& filePath);
    void toggleProjection();
    
protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    
private:
    RhiRenderer*         m_renderer  = nullptr;
    Camera               m_camera;
    Material             m_material;
    Light                m_light;
    QVector<RhiMesh*>    m_meshes;
    BoundingBox          m_sceneBbox;
    QPoint               m_lastMousePos;
    Qt::MouseButton      m_activeButton = Qt::NoButton;
};
