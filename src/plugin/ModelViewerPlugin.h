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

#ifndef MODELVIEWER_SRC_PLUGIN_MODELVIEWERPLUGIN_H_
#define MODELVIEWER_SRC_PLUGIN_MODELVIEWERPLUGIN_H_

#include <QObject>
#include <QString>
#include <QtPlugin>

/// @brief C++ ABI 插件接口
///
/// 插件在 `activate()` 中拿到脚本 API 对象（ViewerScriptApi），
/// 通过 QMetaObject::invokeMethod 调用 `setRenderMode`、`setBackground`、
/// `duplicateMesh` 等方法，避免插件与主程序二进制结构强耦合。
class ModelViewerPlugin {
 public:
  virtual ~ModelViewerPlugin() = default;

  /// @brief 插件名称（显示在菜单中）
  virtual QString name() const = 0;

  /// @brief 插件描述
  virtual QString description() const = 0;

  /// @brief 激活插件
  /// @param[in] api 查看器脚本 API 对象（ViewerScriptApi）
  /// @return 是否成功激活
  virtual bool activate(QObject* api) = 0;
};

#define ModelViewerPlugin_iid "com.modelviewer.ModelViewerPlugin/1.0"
Q_DECLARE_INTERFACE(ModelViewerPlugin, ModelViewerPlugin_iid)

#endif  // MODELVIEWER_SRC_PLUGIN_MODELVIEWERPLUGIN_H_
