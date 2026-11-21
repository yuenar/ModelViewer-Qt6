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

#ifndef MODELVIEWER_SRC_CORE_ANIMATION_H_
#define MODELVIEWER_SRC_CORE_ANIMATION_H_

#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>
#include <QVector>
#include <QVector3D>

#include "CpuMesh.h"

/// @brief 单个节点的关键帧动画通道
///
/// 位置/旋转/缩放分别带有时间轴；缺少某个分量的通道会回退到节点的
/// 静止变换（rest pose），保证采样结果始终是完整的局部变换。
struct AnimationChannel {
  QString nodeName;

  QVector<float> positionTimes;
  QVector<QVector3D> positionValues;
  QVector<float> rotationTimes;
  QVector<QQuaternion> rotationValues;
  QVector<float> scaleTimes;
  QVector<QVector3D> scaleValues;

  QVector3D restPosition{0, 0, 0};
  QQuaternion restRotation;
  QVector3D restScale{1, 1, 1};

  /// @brief 在指定时间采样局部 TRS 矩阵
  QMatrix4x4 sample(float time) const;

 private:
  QVector3D sampleVec3(const QVector<float>& times,
                       const QVector<QVector3D>& values, float time,
                       const QVector3D& fallback) const;
  QQuaternion sampleQuat(const QVector<float>& times,
                         const QVector<QQuaternion>& values, float time) const;
};

/// @brief 一段动画（由若干节点通道组成）
struct AnimationClip {
  QString name;
  float duration = 0.0f;
  QVector<AnimationChannel> channels;
};

/// @brief 模型加载结果（网格 + 动画）
struct ModelAsset {
  QVector<CpuMesh> meshes;
  QVector<AnimationClip> animations;
};

#endif  // MODELVIEWER_SRC_CORE_ANIMATION_H_
