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

#ifndef MODELVIEWER_SRC_UI_TOOLEDITOR_H_
#define MODELVIEWER_SRC_UI_TOOLEDITOR_H_

#include <QDialog>
#include <QString>
#include <functional>

class QLabel;
class QListWidget;
class QPlainTextEdit;

/// @brief 自定义工具编辑器
///
/// 管理 `plugins/*.js` 脚本工具：新建、编辑、保存、删除并直接运行。
/// 脚本通过 `viewer.*` API 控制场景，与脚本控制台共用同一引擎。
class ToolEditor : public QDialog {
  Q_OBJECT

 public:
  using ScriptRunner = std::function<bool(const QString& path)>;

  ToolEditor(const QString& toolDirectory, ScriptRunner runner,
             QWidget* parent = nullptr);

 private:
  void refreshList();
  void loadCurrent();
  void newTool();
  void saveCurrent();
  void runCurrent();
  void deleteCurrent();
  void appendLog(const QString& text);

  QString m_directory;
  ScriptRunner m_runner;
  QListWidget* m_list = nullptr;
  QPlainTextEdit* m_editor = nullptr;
  QPlainTextEdit* m_log = nullptr;
  QLabel* m_status = nullptr;
  QString m_currentPath;
};

#endif  // MODELVIEWER_SRC_UI_TOOLEDITOR_H_
