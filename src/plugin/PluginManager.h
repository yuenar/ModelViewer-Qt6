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

#ifndef MODELVIEWER_SRC_PLUGIN_PLUGINMANAGER_H_
#define MODELVIEWER_SRC_PLUGIN_PLUGINMANAGER_H_

#include <QList>
#include <QObject>
#include <QStringList>

class QPluginLoader;

/// @brief C++ 原生插件管理器
///
/// 扫描可执行文件旁的 `plugins/` 目录，加载实现了 ModelViewerPlugin
/// 接口的动态库，并用脚本 API 对象激活它们。
class PluginManager : public QObject {
  Q_OBJECT

 public:
  explicit PluginManager(QObject* parent = nullptr);
  ~PluginManager() override;

  /// @brief 卸载并重新加载所有原生插件
  int loadAll(QObject* api);

  QStringList pluginNames() const { return m_names; }
  QStringList failedPlugins() const { return m_failed; }
  QString statusText() const;

 private:
  void unloadAll();

  QList<QPluginLoader*> m_loaders;
  QStringList m_names;
  QStringList m_failed;
};

#endif  // MODELVIEWER_SRC_PLUGIN_PLUGINMANAGER_H_
