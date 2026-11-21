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
    view.lookAt(m_eye, m_pivot, m_up.normalized());
    return view;
}

QMatrix4x4 Camera::projectionMatrix() const {
    return projectionMatrix(m_aspectRatio);
}

QMatrix4x4 Camera::projectionMatrix(float aspect) const {
    QMatrix4x4 proj;

    const float dist = qMax((m_eye - m_pivot).length(), 0.001f);
    const float nearPlane = qMax(0.001f, dist * 0.001f);
    const float farPlane = qMax(1000.0f, dist * 200.0f);

    if (m_projType == ProjectionType::Perspective) {
        proj.perspective(m_fov, aspect, nearPlane, farPlane);
    } else {
        const float height = 2.0f * dist * qTan(qDegreesToRadians(m_fov * 0.5f));
        const float width = height * aspect;
        proj.ortho(-width / 2, width / 2, -height / 2, height / 2,
                   nearPlane, farPlane);
    }

    return proj;
}

void Camera::setAspectRatio(float aspect) {
    if (aspect > 0.0f) {
        m_aspectRatio = aspect;
    }
}

void Camera::setProjectionType(ProjectionType type) {
    m_projType = type;
}

void Camera::beginRotate(const QPoint& point) {
    m_lastRotatePoint = point;
}

void Camera::rotate(const QPoint& from, const QPoint& to, const QSize& viewport) {
    if (viewport.width() <= 0 || viewport.height() <= 0) {
        return;
    }

    const QPointF fromNorm(float(from.x()) / viewport.width(),
                           float(from.y()) / viewport.height());
    const QPointF toNorm(float(to.x()) / viewport.width(),
                         float(to.y()) / viewport.height());

    const QQuaternion delta = m_trackball->deltaRotation(fromNorm, toNorm);
    if (delta.isIdentity()) {
        return;
    }

    // 相机绕 pivot 旋转：位置与上方向一起旋转，模型矩阵保持单位阵
    m_eye = m_pivot + delta.rotatedVector(m_eye - m_pivot);
    m_up = delta.rotatedVector(m_up).normalized();
}

void Camera::pan(const QPoint& delta) {
    const QVector3D forward = (m_pivot - m_eye).normalized();
    QVector3D right = QVector3D::crossProduct(forward, m_up);
    if (right.lengthSquared() < 1e-8f) {
        right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0));
    }
    right.normalize();
    const QVector3D up = QVector3D::crossProduct(right, forward).normalized();

    const float dist = qMax((m_eye - m_pivot).length(), 0.001f);
    const float scale = dist * 0.0015f;

    const QVector3D offset = right * (-delta.x() * scale) + up * (delta.y() * scale);
    m_pivot += offset;
    m_eye += offset;
}

void Camera::zoom(float delta) {
    const float dist = (m_eye - m_pivot).length();
    const float scale = qBound(0.05f, 1.0f + delta * 0.1f, 20.0f);
    const float newDist = qBound(0.001f, dist * scale, 1.0e7f);
    if (dist > 1e-6f) {
        m_eye = m_pivot + (m_eye - m_pivot) * (newDist / dist);
    }
}

void Camera::fitToView(const BoundingBox& bbox) {
    if (bbox.isEmpty()) {
        return;
    }

    m_pivot = bbox.center();
    const float radius = qMax(bbox.radius(), 0.001f);
    const float distance = radius / qMax(qSin(qDegreesToRadians(m_fov * 0.5f)), 0.05f);

    // 保持当前观察方向，避免每次适配都把视角重置到 +Z
    QVector3D dir = m_eye - m_pivot;
    if (dir.lengthSquared() < 1e-8f) {
        dir = QVector3D(0, 0, 1);
    }
    dir.normalize();
    m_eye = m_pivot + dir * distance;
}

void Camera::setViewPreset(int preset, const BoundingBox& bbox) {
    if (bbox.isEmpty()) {
        return;
    }

    m_pivot = bbox.center();
    const float radius = qMax(bbox.radius(), 0.001f);
    const float distance = radius / qMax(qSin(qDegreesToRadians(m_fov * 0.5f)), 0.05f) * 1.1f;

    QVector3D dir(0, 0, 1);
    QVector3D up(0, 1, 0);

    switch (preset) {
        case 0: dir = QVector3D(0, 0, 1); break;            // 前
        case 1: dir = QVector3D(0, 0, -1); break;           // 后
        case 2: dir = QVector3D(-1, 0, 0); break;           // 左
        case 3: dir = QVector3D(1, 0, 0); break;            // 右
        case 4: dir = QVector3D(0, 1, 0); up = QVector3D(0, 0, -1); break;   // 顶
        case 5: dir = QVector3D(0, -1, 0); up = QVector3D(0, 0, 1); break;   // 底
        case 7: dir = QVector3D(1, 1, 2); break;            // 二轴测
        case 8: dir = QVector3D(1, 2, 3); break;            // 三轴测
        case 6:
        default: dir = QVector3D(1, 1, 1); break;           // 等轴测
    }

    m_eye = m_pivot + dir.normalized() * distance;
    m_up = up.normalized();
}

void Camera::setPosition(const QVector3D& pos) {
    m_eye = pos;
}

void Camera::setTarget(const QVector3D& target) {
    m_pivot = target;
}

void Camera::setUp(const QVector3D& up) {
    if (up.lengthSquared() > 1e-8f) {
        m_up = up.normalized();
    }
}

void Camera::reset() {
    m_eye = QVector3D(0, 0, 5);
    m_pivot = QVector3D(0, 0, 0);
    m_up = QVector3D(0, 1, 0);
    m_trackball->reset();
}
