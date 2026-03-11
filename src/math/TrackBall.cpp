#include "TrackBall.h"
#include <QtMath>

void TrackBall::press(const QPointF& point) {
    m_lastPos = point;
    m_pressed = true;
}

void TrackBall::move(const QPointF& point) {
    if (!m_pressed) return;
    
    const QVector3D from = mapToSphere(m_lastPos);
    const QVector3D to = mapToSphere(point);
    
    const QVector3D axis = QVector3D::crossProduct(from, to);
    const float angle = std::acos(QVector3D::dotProduct(from, to));
    
    m_rotation = QQuaternion::fromAxisAndAngle(axis, qRadiansToDegrees(angle)) * m_rotation;
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
    const float x = (p.x() - 0.5f) * 2.0f;
    const float y = (p.y() - 0.5f) * 2.0f;
    
    const float length2 = x*x + y*y;
    if (length2 <= radius*radius) {
        return {x, y, std::sqrt(radius*radius - length2)};
    } else {
        const float length = std::sqrt(length2);
        return {x/length, y/length, 0.0f};
    }
}
