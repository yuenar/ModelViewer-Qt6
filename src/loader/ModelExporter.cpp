#include "ModelExporter.h"

#include <QSaveFile>
#include <QTextStream>
#include <QVector3D>

namespace {

void setError(QString* errorMessage, const QString& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

}  // namespace

namespace ModelExporter {

bool exportOBJ(const QVector<CpuMesh>& meshes, const QString& path,
               QString* errorMessage) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, file.errorString());
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(6);
    out.setRealNumberNotation(QTextStream::FixedNotation);

    out << "# ModelViewer Qt6 RHI export\n";

    quint32 vertexOffset = 1;
    int objectIndex = 0;

    for (const CpuMesh& mesh : meshes) {
        const QString name = mesh.name.isEmpty()
            ? QString("Mesh_%1").arg(objectIndex)
            : mesh.name;
        out << "o " << name << "\n";

        for (const auto& vertex : mesh.vertices) {
            out << "v " << vertex.position.x() << ' ' << vertex.position.y()
                << ' ' << vertex.position.z() << "\n";
            out << "vn " << vertex.normal.x() << ' ' << vertex.normal.y()
                << ' ' << vertex.normal.z() << "\n";
        }

        for (int i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const quint32 a = vertexOffset + mesh.indices[i];
            const quint32 b = vertexOffset + mesh.indices[i + 1];
            const quint32 c = vertexOffset + mesh.indices[i + 2];
            out << "f " << a << "//" << a << ' '
                << b << "//" << b << ' '
                << c << "//" << c << "\n";
        }

        vertexOffset += quint32(mesh.vertices.size());
        ++objectIndex;
    }

    out.flush();
    if (file.error() != QFileDevice::NoError) {
        setError(errorMessage, file.errorString());
        return false;
    }
    return file.commit();
}

bool exportSTL(const QVector<CpuMesh>& meshes, const QString& path,
               QString* errorMessage) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, file.errorString());
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(6);
    out.setRealNumberNotation(QTextStream::FixedNotation);

    out << "solid ModelViewer\n";

    for (const CpuMesh& mesh : meshes) {
        for (int i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const QVector3D& p0 = mesh.vertices[mesh.indices[i]].position;
            const QVector3D& p1 = mesh.vertices[mesh.indices[i + 1]].position;
            const QVector3D& p2 = mesh.vertices[mesh.indices[i + 2]].position;

            QVector3D normal = QVector3D::crossProduct(p1 - p0, p2 - p0);
            if (normal.lengthSquared() > 1e-12f) {
                normal.normalize();
            } else {
                normal = QVector3D(0, 1, 0);
            }

            out << "  facet normal " << normal.x() << ' ' << normal.y()
                << ' ' << normal.z() << "\n";
            out << "    outer loop\n";
            out << "      vertex " << p0.x() << ' ' << p0.y() << ' ' << p0.z() << "\n";
            out << "      vertex " << p1.x() << ' ' << p1.y() << ' ' << p1.z() << "\n";
            out << "      vertex " << p2.x() << ' ' << p2.y() << ' ' << p2.z() << "\n";
            out << "    endloop\n";
            out << "  endfacet\n";
        }
    }

    out << "endsolid ModelViewer\n";
    out.flush();

    if (file.error() != QFileDevice::NoError) {
        setError(errorMessage, file.errorString());
        return false;
    }
    return file.commit();
}

}  // namespace ModelExporter
