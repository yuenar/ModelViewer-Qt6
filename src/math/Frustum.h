// Copyright 2026 ModelViewer Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MODELVIEWER_SRC_MATH_FRUSTUM_H_
#define MODELVIEWER_SRC_MATH_FRUSTUM_H_

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

/// @brief 视锥体（6 个裁剪平面），用于快速剔除不可见对象
///
/// 平面由 view-projection 矩阵按 Gribb-Hartmann 方法提取，
/// 约定 dot(plane.xyz, pos) + plane.w >= 0 为可见侧。
struct Frustum {
  QVector4D planes[6];

  /// @brief 从 view-projection 矩阵构建视锥体
  ///
  /// 近平面采用 OpenGL 约定（row3 + row2），对 0..1 深度范围的后端
  /// 只会导致近平面偏宽，剔除结果仍然保守正确（不会误剔除）。
  static Frustum fromMatrix(const QMatrix4x4& viewProj) {
    Frustum f;
    const QVector4D row0 = viewProj.row(0);
    const QVector4D row1 = viewProj.row(1);
    const QVector4D row2 = viewProj.row(2);
    const QVector4D row3 = viewProj.row(3);

    f.planes[0] = row3 + row0;   // left
    f.planes[1] = row3 - row0;   // right
    f.planes[2] = row3 + row1;   // bottom
    f.planes[3] = row3 - row1;   // top
    f.planes[4] = row3 + row2;   // near
    f.planes[5] = row3 - row2;   // far
    return f;
  }

  /// @brief 包围球与视锥体的相交测试
  bool intersectsSphere(const QVector3D& center, float radius) const {
    for (int i = 0; i < 6; ++i) {
      const QVector4D& p = planes[i];
      const float distance = p.x() * center.x() + p.y() * center.y() +
                             p.z() * center.z() + p.w();
      if (distance < -radius) {
        return false;
      }
    }
    return true;
  }
};

#endif  // MODELVIEWER_SRC_MATH_FRUSTUM_H_
