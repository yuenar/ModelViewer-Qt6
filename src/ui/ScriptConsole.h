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

#ifndef MODELVIEWER_SRC_UI_SCRIPTCONSOLE_H_
#define MODELVIEWER_SRC_UI_SCRIPTCONSOLE_H_

#include <QDialog>
#include <QJSEngine>
#include <QJSValue>
#include <QObject>
#include <QString>

class QLineEdit;
class QPlainTextEdit;
class RhiWidget;

/// @brief 暴露给脚本引擎的查看器控制接口
///
/// 在脚本中通过全局对象 `viewer` 访问，例如：
///   viewer.setRenderMode(6);
///   viewer.setBackground(0.1, 0.1, 0.2);
///   viewer.duplicateMesh(0);
class ViewerScriptApi : public QObject {
  Q_OBJECT

 public:
  explicit ViewerScriptApi(RhiWidget* view, QObject* parent = nullptr);

  void setView(RhiWidget* view) { m_view = view; }

  Q_INVOKABLE int meshCount() const;
  Q_INVOKABLE QString meshName(int index) const;
  Q_INVOKABLE void selectMesh(int index);
  Q_INVOKABLE void setMeshVisible(int index, bool visible);

  Q_INVOKABLE void setRenderMode(int mode);
  Q_INVOKABLE void setDisplayStyle(int style);
  Q_INVOKABLE void setBackground(double r, double g, double b);
  Q_INVOKABLE void setAxisVisible(bool visible);
  Q_INVOKABLE void setGridVisible(bool visible);
  Q_INVOKABLE void setMultiView(bool enabled);
  Q_INVOKABLE void setCulling(bool enabled);
  Q_INVOKABLE void setLod(bool enabled);
  Q_INVOKABLE void setMsaa(int samples);
  Q_INVOKABLE void setPostProcessing(bool fxaa, bool ao);
  Q_INVOKABLE void viewPreset(int preset);
  Q_INVOKABLE void fitView();

  Q_INVOKABLE void setMeshColor(int index, double r, double g, double b);
  Q_INVOKABLE void setMeshTransform(int index, double tx, double ty, double tz,
                                    double rx, double ry, double rz, double sx,
                                    double sy, double sz);
  Q_INVOKABLE int duplicateMesh(int index);
  Q_INVOKABLE bool deleteMesh(int index);
  Q_INVOKABLE bool renameMesh(int index, const QString& name);

  Q_INVOKABLE int animationCount() const;
  Q_INVOKABLE QString animationName(int index) const;
  Q_INVOKABLE void playAnimation(int index);
  Q_INVOKABLE void stopAnimation();
  Q_INVOKABLE void setAnimationTime(double seconds);

  Q_INVOKABLE QString stats() const;

 private:
  RhiWidget* m_view = nullptr;
};

/// @brief 脚本 console.log 桥接对象
class ScriptConsoleBridge : public QObject {
  Q_OBJECT

 public:
  explicit ScriptConsoleBridge(class ScriptConsole* console)
      : m_console(console) {}

 public slots:
  void log(const QString& text);

 private:
  class ScriptConsole* m_console = nullptr;
};

/// @brief 脚本控制台（支持 .js 插件与交互式命令）
class ScriptConsole : public QDialog {
  Q_OBJECT

 public:
  explicit ScriptConsole(RhiWidget* view, QWidget* parent = nullptr);

  void setView(RhiWidget* view);

  /// @brief 执行一段脚本，错误/返回值输出到控制台
  bool runScript(const QString& code, const QString& label = "script");

  /// @brief 执行脚本文件（用于插件与命令行自动化）
  bool runFile(const QString& path);

  void appendOutput(const QString& text);

 private:
  QJSEngine m_engine;
  ViewerScriptApi* m_api = nullptr;
  QPlainTextEdit* m_output = nullptr;
  QLineEdit* m_input = nullptr;
};

#endif  // MODELVIEWER_SRC_UI_SCRIPTCONSOLE_H_
