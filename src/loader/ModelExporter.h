#ifndef MODELVIEWER_SRC_LOADER_MODELEXPORTER_H_
#define MODELVIEWER_SRC_LOADER_MODELEXPORTER_H_

#include <QString>
#include <QVector>
#include "../core/CpuMesh.h"

/// @brief 将 CpuMesh 导出为常见格式
namespace ModelExporter {

bool exportOBJ(const QVector<CpuMesh>& meshes, const QString& path,
               QString* errorMessage = nullptr);
bool exportSTL(const QVector<CpuMesh>& meshes, const QString& path,
               QString* errorMessage = nullptr);

}  // namespace ModelExporter

#endif  // MODELVIEWER_SRC_LOADER_MODELEXPORTER_H_
