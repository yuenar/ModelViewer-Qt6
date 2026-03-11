#pragma once
#include <QVector3D>

struct Material {
    QVector3D ambient{0.2f, 0.2f, 0.2f};
    QVector3D diffuse{0.8f, 0.8f, 0.8f};
    QVector3D specular{0.1f, 0.1f, 0.1f};
    float shininess = 32.0f;
};
