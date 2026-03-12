#include "Camera.h"
#include "TrackBall.h"
#include <QtMath>

Camera::Camera() : m_trackball(new TrackBall()) {
}

Camera::~Camera() {
    delete m_trackball;
}

QMatrix4x4 Camera::viewMatrix() const {
    QMatrix4x4 view;
    view.lookAt(m_eye, m_pivot, m_up);
    
    // 应用轨迹球旋转
    QMatrix4x4 rot;
    rot.rotate(m_trackball->rotation());
    
    return view * rot;
}

QMatrix4x4 Camera::projectionMatrix() const {
    QMatrix4x4 proj;
    
    if (m_projType == ProjectionType::Perspective) {
        proj.perspective(m_fov, m_aspectRatio, m_near, m_far);
    } else {
        const float height = 2.0f * (m_eye - m_pivot).length() * qTan(qDegreesToRadians(m_fov * 0.5f));
        const float width = height * m_aspectRatio;
        proj.ortho(-width/2, width/2, -height/2, height/2, m_near, m_far);
    }
    
    return proj;
}

void Camera::setAspectRatio(float aspect) {
    m_aspectRatio = aspect;
}

void Camera::setProjectionType(ProjectionType type) {
    m_projType = type;
}

void Camera::beginRotate(const QPoint& point) {
    // 暂时跳过轨迹球初始化，在rotate方法中处理
    m_lastRotatePoint = point;
}

void Camera::rotate(const QPoint& from, const QPoint& to, const QSize& viewport) {
    const QPointF fromNorm(float(from.x()) / viewport.width(), 
                          float(from.y()) / viewport.height());
    const QPointF toNorm(float(to.x()) / viewport.width(), 
                        float(to.y()) / viewport.height());
    
    m_trackball->press(fromNorm);
    m_trackball->move(toNorm);
}

void Camera::pan(const QPoint& delta) {
    const float distance = (m_eye - m_pivot).length();
    const float scale = distance * 0.001f;
    
    const QVector3D right = QVector3D::crossProduct(m_pivot - m_eye, {0, 1, 0}).normalized();
    const QVector3D up = {0, 1, 0};
    
    m_pivot -= right * delta.x() * scale;
    m_pivot += up * delta.y() * scale;
    m_eye -= right * delta.x() * scale;
    m_eye += up * delta.y() * scale;
}

void Camera::zoom(float delta) {
    const float scale = 1.0f + delta * 0.1f;
    m_eye = m_pivot + (m_eye - m_pivot) * scale;
}

void Camera::fitToView(const BoundingBox& bbox) {
    if (bbox.isEmpty()) return;
    
    m_pivot = bbox.center();
    const float radius = bbox.radius();
    
    const float distance = radius / qSin(qDegreesToRadians(m_fov * 0.5f));
    m_eye = m_pivot + QVector3D(0, 0, distance);
    
    m_trackball->reset();
}


void Camera::setPosition(const QVector3D& pos) {
    m_eye = pos;
    m_trackball->reset();
}

void Camera::setTarget(const QVector3D& target) {
    m_pivot = target;
}

void Camera::setUp(const QVector3D& up) {
    m_up = up.normalized();
}

void Camera::reset() {
    m_eye = QVector3D(0, 0, 5);
    m_pivot = QVector3D(0, 0, 0);
    m_up = QVector3D(0, 1, 0);
    m_trackball->reset();
}
