#pragma once
#include <QString>
#include <QVector>
#include "../core/CpuMesh.h"

class ModelLoader {
public:
    QVector<CpuMesh> loadCPU(const QString& filePath);
    
private:
    QVector<CpuMesh> loadWithAssimp(const QString& path);
    bool isAssimpFormat(const QString& ext) const;
};
