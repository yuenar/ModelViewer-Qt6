#pragma once
#include <QVector3D>

class BoundingBox {
public:
    BoundingBox() = default;
    
    void reset();
    void expand(const QVector3D& point);
    void expand(const BoundingBox& other);
    
    QVector3D center() const;
    QVector3D size() const;
    float radius() const;
    
    bool isEmpty() const;
    
private:
    QVector3D m_min{FLT_MAX, FLT_MAX, FLT_MAX};
    QVector3D m_max{-FLT_MAX, -FLT_MAX, -FLT_MAX};
};
