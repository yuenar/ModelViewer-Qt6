#pragma once
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include "BoundingBox.h"

enum class ProjectionType {
    Perspective,
    Orthographic
};

class TrackBall;

class Camera {
public:
    Camera();
    ~Camera();
    
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    QVector3D position() const { return m_eye; }
    
    void setAspectRatio(float aspect);
    void setProjectionType(ProjectionType type);
    
    // 交互接口
    void beginRotate(const QPoint& point);
    void rotate(const QPoint& from, const QPoint& to, const QSize& viewport);
    void pan(const QPoint& delta);
    void zoom(float delta);
    void fitToView(const BoundingBox& bbox);
    
private:
    TrackBall* m_trackball = nullptr;
    QVector3D m_pivot{0, 0, 0};
    QVector3D m_eye{0, 0, 5};
    float m_fov = 45.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
    float m_aspectRatio = 1.0f;
    ProjectionType m_projType = ProjectionType::Perspective;
    QPoint m_lastRotatePoint;
};
