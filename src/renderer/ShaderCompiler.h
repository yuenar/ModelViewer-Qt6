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

#ifndef MODELVIEWER_SRC_RENDERER_SHADERCOMPILER_H_
#define MODELVIEWER_SRC_RENDERER_SHADERCOMPILER_H_

#include <QByteArray>
#include <QString>

/// @brief 在运行时把 GLSL 源码烘焙为多后端 QSB
///
/// 与构建期的 qt_add_shaders 目标保持一致：GLSL 460 / HLSL 50 / MSL 12 /
/// SPIR-V 1.0。失败时返回 false 并通过 error 输出编译日志。
bool bakeShaderSource(const QString& source, const QString& fileName,
                      QByteArray* serialized, QString* error);

/// @brief 根据文件名推断着色器阶段（.vert/.frag 等）
bool shaderStageFromFileName(const QString& fileName, int* stage);

#endif  // MODELVIEWER_SRC_RENDERER_SHADERCOMPILER_H_
