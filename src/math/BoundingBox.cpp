#include "BoundingBox.h"

void BoundingBox::reset() {
    m_min = {FLT_MAX, FLT_MAX, FLT_MAX};
    m_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
}

void BoundingBox::expand(const QVector3D& point) {
    if (isEmpty()) {
        m_min = m_max = point;
    } else {
        m_min.setX(std::min(m_min.x(), point.x()));
        m_min.setY(std::min(m_min.y(), point.y()));
        m_min.setZ(std::min(m_min.z(), point.z()));
        
        m_max.setX(std::max(m_max.x(), point.x()));
        m_max.setY(std::max(m_max.y(), point.y()));
        m_max.setZ(std::max(m_max.z(), point.z()));
    }
}

void BoundingBox::expand(const BoundingBox& other) {
    if (!other.isEmpty()) {
        expand(other.m_min);
        expand(other.m_max);
    }
}

QVector3D BoundingBox::center() const {
    if (isEmpty()) return {};
    return (m_min + m_max) * 0.5f;
}

QVector3D BoundingBox::size() const {
    if (isEmpty()) return {};
    return m_max - m_min;
}

float BoundingBox::radius() const {
    if (isEmpty()) return 0.0f;
    return size().length() * 0.5f;
}

bool BoundingBox::isEmpty() const {
    return m_min.x() > m_max.x() || m_min.y() > m_max.y() || m_min.z() > m_max.z();
}
