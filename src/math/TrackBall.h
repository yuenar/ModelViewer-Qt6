#pragma once
#include <QQuaternion>
#include <QPointF>

class TrackBall {
public:
    TrackBall() = default;
    
    void press(const QPointF& point);
    void move(const QPointF& point);
    QQuaternion rotation() const;
    void reset();
    
private:
    QVector3D mapToSphere(const QPointF& p) const;
    
    QQuaternion m_rotation;
    QPointF m_lastPos;
    bool m_pressed = false;
};
