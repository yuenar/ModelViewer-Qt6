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

#ifndef MODELVIEWER_SRC_UI_SHADEREDITOR_H_
#define MODELVIEWER_SRC_UI_SHADEREDITOR_H_

#include <QDialog>
#include <QString>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class RhiWidget;

/// @brief 运行时着色器编辑器
///
/// 编辑 `shaders/` 下的 GLSL 源码，使用 QShaderBaker 现场编译为多后端 QSB，
/// 覆盖对应资源并在下一帧重建渲染管线；无需重新编译整个程序。
class ShaderEditor : public QDialog {
  Q_OBJECT

 public:
  explicit ShaderEditor(RhiWidget* view, QWidget* parent = nullptr);

  /// @brief 编译并应用一个着色器源文件（供命令行/自动化复用）
  static bool compileAndApply(RhiWidget* view, const QString& sourcePath,
                              QString* error);

  /// @brief 查找运行时着色器源码目录
  static QString shaderSourceDirectory();

 private:
  void refreshList();
  void loadCurrent();
  void saveCurrent();
  void compileCurrent();
  void restoreCurrent();
  void appendLog(const QString& text);

  RhiWidget* m_view = nullptr;
  QListWidget* m_list = nullptr;
  QPlainTextEdit* m_editor = nullptr;
  QPlainTextEdit* m_log = nullptr;
  QLabel* m_status = nullptr;
  QString m_currentPath;
};

#endif  // MODELVIEWER_SRC_UI_SHADEREDITOR_H_
