#ifndef MODELVIEWER_SRC_CORE_PARAMETRICSURFACE_H_
#define MODELVIEWER_SRC_CORE_PARAMETRICSURFACE_H_

#include "CpuMesh.h"

/// @brief 程序化参数曲面生成器
namespace ParametricSurface {

CpuMesh plane(float size = 2.0f, int segments = 16);
CpuMesh sphere(float radius = 1.0f, int slices = 40, int stacks = 20);
CpuMesh torus(float majorRadius = 1.0f, float minorRadius = 0.35f,
              int slices = 48, int stacks = 24);
CpuMesh cylinder(float radius = 0.6f, float height = 2.0f, int slices = 40);
CpuMesh cone(float radius = 0.8f, float height = 2.0f, int slices = 40);

}  // namespace ParametricSurface

#endif  // MODELVIEWER_SRC_CORE_PARAMETRICSURFACE_H_
