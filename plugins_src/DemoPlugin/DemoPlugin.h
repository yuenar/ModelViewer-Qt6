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

#ifndef MODELVIEWER_PLUGINS_DEMOPLUGIN_H_
#define MODELVIEWER_PLUGINS_DEMOPLUGIN_H_

#include "../../src/plugin/ModelViewerPlugin.h"

#include <QMetaObject>

/// @brief 示例 C++ 插件：切换到 PBR + Realistic 并设置蓝色背景
class DemoPlugin : public QObject, public ModelViewerPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID ModelViewerPlugin_iid)
  Q_INTERFACES(ModelViewerPlugin)

 public:
  QString name() const override { return "Demo PBR Plugin"; }

  QString description() const override {
    return "Switches to PBR/Realistic and applies a blue background";
  }

  bool activate(QObject* api) override {
    if (!api) {
      return false;
    }
    QMetaObject::invokeMethod(api, "setRenderMode", Qt::DirectConnection,
                              Q_ARG(int, 6));
    QMetaObject::invokeMethod(api, "setDisplayStyle", Qt::DirectConnection,
                              Q_ARG(int, 3));
    QMetaObject::invokeMethod(api, "setBackground", Qt::DirectConnection,
                              Q_ARG(double, 0.08), Q_ARG(double, 0.10),
                              Q_ARG(double, 0.35));
    return true;
  }
};

#endif  // MODELVIEWER_PLUGINS_DEMOPLUGIN_H_
