#include "TrackBall.h"
#include <QtMath>

void TrackBall::press(const QPointF& point) {
    m_lastPos = point;
    m_pressed = true;
}

QQuaternion TrackBall::deltaRotation(const QPointF& from, const QPointF& to) const {
    const QVector3D vFrom = mapToSphere(from);
    const QVector3D vTo = mapToSphere(to);

    QVector3D axis = QVector3D::crossProduct(vFrom, vTo);
    if (axis.lengthSquared() < 1e-10f) {
        return QQuaternion();
    }
    axis.normalize();

    const float dot = qBound(-1.0f, QVector3D::dotProduct(vFrom, vTo), 1.0f);
    const float angle = std::acos(dot);
    return QQuaternion::fromAxisAndAngle(axis, qRadiansToDegrees(angle));
}

void TrackBall::move(const QPointF& point) {
    if (!m_pressed) {
        return;
    }

    m_rotation = deltaRotation(m_lastPos, point) * m_rotation;
    m_lastPos = point;
}

QQuaternion TrackBall::rotation() const {
    return m_rotation;
}

void TrackBall::reset() {
    m_rotation = QQuaternion();
    m_pressed = false;
}

QVector3D TrackBall::mapToSphere(const QPointF& p) const {
    const float radius = 1.0f;
    // 屏幕坐标 y 向下，球面坐标 y 向上，需要翻转
    const float x = (p.x() - 0.5f) * 2.0f;
    const float y = (0.5f - p.y()) * 2.0f;

    const float length2 = x * x + y * y;
    if (length2 <= radius * radius) {
        return {x, y, std::sqrt(radius * radius - length2)};
    }
    const float length = std::sqrt(length2);
    return {x / length, y / length, 0.0f};
}
