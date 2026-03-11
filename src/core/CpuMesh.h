#pragma once
#include <QVector3D>
#include <QVector2D>
#include "../math/BoundingBox.h"

struct CpuMesh {
    struct Vertex {
        QVector3D position;
        QVector3D normal;
        QVector2D texCoord;
    };
    
    QVector<Vertex> vertices;
    QVector<quint32> indices;
    BoundingBox bbox;
};
