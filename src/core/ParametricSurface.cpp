#include "ParametricSurface.h"

#include <QVector3D>
#include <QVector>
#include <QtMath>
#include <functional>

namespace {

using GridFunction = std::function<QVector3D(float u, float v)>;

void computeNormals(CpuMesh& mesh) {
    QVector<QVector3D> normals(mesh.vertices.size(), QVector3D(0, 0, 0));

    for (int t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const quint32 i0 = mesh.indices[t];
        const quint32 i1 = mesh.indices[t + 1];
        const quint32 i2 = mesh.indices[t + 2];
        const QVector3D& p0 = mesh.vertices[i0].position;
        const QVector3D& p1 = mesh.vertices[i1].position;
        const QVector3D& p2 = mesh.vertices[i2].position;
        const QVector3D n = QVector3D::crossProduct(p1 - p0, p2 - p0);
        normals[i0] += n;
        normals[i1] += n;
        normals[i2] += n;
    }

    for (int i = 0; i < mesh.vertices.size(); ++i) {
        if (normals[i].lengthSquared() > 1e-12f) {
            mesh.vertices[i].normal = normals[i].normalized();
        } else {
            mesh.vertices[i].normal = QVector3D(0, 1, 0);
        }
    }
}

float signedVolume(const CpuMesh& mesh) {
    float volume = 0.0f;
    for (int t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const QVector3D& p0 = mesh.vertices[mesh.indices[t]].position;
        const QVector3D& p1 = mesh.vertices[mesh.indices[t + 1]].position;
        const QVector3D& p2 = mesh.vertices[mesh.indices[t + 2]].position;
        volume += QVector3D::dotProduct(p0, QVector3D::crossProduct(p1, p2));
    }
    return volume / 6.0f;
}

void fixWinding(CpuMesh& mesh, bool closedVolume) {
    if (!closedVolume) {
        return;
    }
    if (signedVolume(mesh) >= 0.0f) {
        return;
    }

    for (int t = 0; t + 2 < mesh.indices.size(); t += 3) {
        std::swap(mesh.indices[t + 1], mesh.indices[t + 2]);
    }
    for (auto& vertex : mesh.vertices) {
        vertex.normal = -vertex.normal;
    }
}

void updateBounds(CpuMesh& mesh) {
    mesh.bbox = BoundingBox();
    for (const auto& vertex : mesh.vertices) {
        mesh.bbox.expand(vertex.position);
    }
}

CpuMesh buildGrid(int nu, int nv, const GridFunction& function,
                  const QString& name, bool closedVolume) {
    CpuMesh mesh;
    mesh.name = name;
    mesh.baseColor = QVector3D(0.75f, 0.78f, 0.82f);

    mesh.vertices.reserve((nu + 1) * (nv + 1));
    for (int j = 0; j <= nv; ++j) {
        for (int i = 0; i <= nu; ++i) {
            CpuMesh::Vertex vertex;
            vertex.position = function(float(i) / float(nu), float(j) / float(nv));
            mesh.vertices.append(vertex);
        }
    }

    auto indexAt = [nu](int i, int j) {
        return quint32(j * (nu + 1) + i);
    };

    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const quint32 a = indexAt(i, j);
            const quint32 b = indexAt(i + 1, j);
            const quint32 c = indexAt(i + 1, j + 1);
            const quint32 d = indexAt(i, j + 1);
            mesh.indices << a << b << c;
            mesh.indices << a << c << d;
        }
    }

    computeNormals(mesh);
    fixWinding(mesh, closedVolume);
    updateBounds(mesh);
    return mesh;
}

void reverseTriangleWinding(CpuMesh& mesh) {
    for (int t = 0; t + 2 < mesh.indices.size(); t += 3) {
        std::swap(mesh.indices[t + 1], mesh.indices[t + 2]);
    }
}

void appendCap(CpuMesh& mesh, float y, float radius, int slices, bool top) {
    const quint32 centerIndex = quint32(mesh.vertices.size());
    CpuMesh::Vertex center;
    center.position = QVector3D(0, y, 0);
    mesh.vertices.append(center);

    const quint32 ringStart = quint32(mesh.vertices.size());
    for (int i = 0; i <= slices; ++i) {
        const float angle = 2.0f * float(M_PI) * float(i) / float(slices);
        CpuMesh::Vertex vertex;
        vertex.position = QVector3D(radius * qCos(angle), y, radius * qSin(angle));
        mesh.vertices.append(vertex);
    }

    for (int i = 0; i < slices; ++i) {
        const quint32 a = ringStart + quint32(i);
        const quint32 b = ringStart + quint32(i + 1);
        if (top) {
            mesh.indices << centerIndex << b << a;
        } else {
            mesh.indices << centerIndex << a << b;
        }
    }
}

}  // namespace

namespace ParametricSurface {

CpuMesh plane(float size, int segments) {
    const float half = size * 0.5f;
    return buildGrid(segments, segments,
                     [half](float u, float v) {
                         return QVector3D((u - 0.5f) * 2.0f * half,
                                          (v - 0.5f) * 2.0f * half,
                                          0.0f);
                     },
                     "Plane", false);
}

CpuMesh sphere(float radius, int slices, int stacks) {
    return buildGrid(slices, stacks,
                     [radius](float u, float v) {
                         const float theta = 2.0f * float(M_PI) * u;
                         const float phi = float(M_PI) * v;
                         return QVector3D(radius * qSin(phi) * qCos(theta),
                                          radius * qCos(phi),
                                          radius * qSin(phi) * qSin(theta));
                     },
                     "Sphere", true);
}

CpuMesh torus(float majorRadius, float minorRadius, int slices, int stacks) {
    return buildGrid(slices, stacks,
                     [majorRadius, minorRadius](float u, float v) {
                         const float theta = 2.0f * float(M_PI) * u;
                         const float phi = 2.0f * float(M_PI) * v;
                         const float r = majorRadius + minorRadius * qCos(phi);
                         return QVector3D(r * qCos(theta),
                                          minorRadius * qSin(phi),
                                          r * qSin(theta));
                     },
                     "Torus", true);
}

CpuMesh cylinder(float radius, float height, int slices) {
    const float halfHeight = height * 0.5f;
    CpuMesh mesh = buildGrid(slices, 1,
                             [radius, halfHeight](float u, float v) {
                                 const float angle = 2.0f * float(M_PI) * u;
                                 return QVector3D(radius * qCos(angle),
                                                  (v - 0.5f) * 2.0f * halfHeight,
                                                  radius * qSin(angle));
                             },
                             "Cylinder", false);

    // 侧面网格的初始绕序朝内，先反转再添加端盖
    reverseTriangleWinding(mesh);
    appendCap(mesh, halfHeight, radius, slices, true);
    appendCap(mesh, -halfHeight, radius, slices, false);

    computeNormals(mesh);
    fixWinding(mesh, true);
    updateBounds(mesh);
    return mesh;
}

CpuMesh cone(float radius, float height, int slices) {
    const float halfHeight = height * 0.5f;
    CpuMesh mesh = buildGrid(slices, 1,
                             [radius, halfHeight](float u, float v) {
                                 const float angle = 2.0f * float(M_PI) * u;
                                 const float r = radius * (1.0f - v);
                                 return QVector3D(r * qCos(angle),
                                                  (v - 0.5f) * 2.0f * halfHeight,
                                                  r * qSin(angle));
                             },
                             "Cone", false);

    reverseTriangleWinding(mesh);
    appendCap(mesh, -halfHeight, radius, slices, false);

    computeNormals(mesh);
    fixWinding(mesh, true);
    updateBounds(mesh);
    return mesh;
}

}  // namespace ParametricSurface
