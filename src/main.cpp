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

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTimer>
#include <QDebug>
#include <cstdio>
#include "ui/MainWindow.h"

/// @brief ModelViewer 应用程序入口点
///
/// 支持的命令行参数（同时用于自动化回归）：
/// --model <file>       启动后加载模型
/// --surface <0-4>      创建参数曲面（平面/球/环/圆柱/圆锥）
/// --mode <0-11>        初始渲染模式
/// --display-style <0-3> 显示风格（Shaded/Wireframe/Wire-Shaded/Realistic）
/// --view <0-8>         视图预设
/// --select <index>     选中网格
/// --multiview          四视口
/// --axis / --grid      显示坐标轴 / 地面网格
/// --tool <0-3>         交互工具（旋转/平移/缩放/选择）
/// --screenshot <file>  延迟截图并退出
/// --export <file>      导出 OBJ/STL 并退出
/// --delay <ms>         截图延迟
int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("ModelViewer");
  QApplication::setApplicationVersion("1.1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("ModelViewer Qt6 RHI");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption modelOption({"m", "model"}, "Model file to load.", "file");
  QCommandLineOption surfaceOption("surface", "Create parametric surface (0-4).", "type", "-1");
  QCommandLineOption modeOption("mode", "Initial render mode (0-11).", "mode", "-1");
  QCommandLineOption styleOption("display-style",
                                 "Display style (0-3): Shaded/Wireframe/Wire-Shaded/Realistic.",
                                 "style", "-1");
  QCommandLineOption viewOption("view", "View preset (0-8).", "preset", "-1");
  QCommandLineOption selectOption("select", "Select mesh by index (-1 clears).", "index", "-1");
  QCommandLineOption multiViewOption("multiview", "Enable 4-viewport mode.");
  QCommandLineOption axisOption("axis", "Show axis.");
  QCommandLineOption gridOption("grid", "Show ground grid.");
  QCommandLineOption toolOption("tool", "Interaction tool (0-3).", "tool", "-1");
  QCommandLineOption transformOption(
      "transform", "Object transform: tx,ty,tz,rx,ry,rz,sx,sy,sz.", "values");
  QCommandLineOption colorOption("mesh-color",
                                 "Selected mesh diffuse color: r,g,b (0..1).", "rgb");
  QCommandLineOption screenshotOption("screenshot", "Save screenshot and exit.", "file");
  QCommandLineOption exportOption("export", "Export model and exit.", "file");
  QCommandLineOption delayOption("delay", "Delay before screenshot (ms).", "ms", "2000");
  QCommandLineOption msaaOption("msaa", "MSAA sample count (1/2/4/8).", "samples", "1");
  QCommandLineOption fxaaOption("fxaa", "Enable FXAA post-processing.");
  QCommandLineOption aoOption("ao", "Enable depth AO post-processing.");
  QCommandLineOption statsOption("stats", "Print render statistics before screenshot.");
  QCommandLineOption animateOption("animate", "Play the first animation clip.");
  QCommandLineOption animTimeOption(
      "anim-time", "Set animation time in seconds (deterministic screenshot).", "sec", "-1");
  QCommandLineOption scriptOption("script", "Run a JavaScript file after load.", "file");
  QCommandLineOption sceneExportOption("scene-export",
                                       "Export scene state JSON and continue.", "file");
  QCommandLineOption sceneImportOption("scene-import",
                                       "Import scene state JSON after load.", "file");
  QCommandLineOption collabHostOption("collab-host",
                                      "Host a collaboration session on a port.", "port");
  QCommandLineOption collabJoinOption("collab-join",
                                      "Join a collaboration session (host:port).", "address");
  QCommandLineOption collabTokenOption("collab-token",
                                       "Collaboration session token.", "token");

  parser.addOption(modelOption);
  parser.addOption(surfaceOption);
  parser.addOption(modeOption);
  parser.addOption(styleOption);
  parser.addOption(viewOption);
  parser.addOption(selectOption);
  parser.addOption(multiViewOption);
  parser.addOption(axisOption);
  parser.addOption(gridOption);
  parser.addOption(toolOption);
  parser.addOption(transformOption);
  parser.addOption(colorOption);
  parser.addOption(screenshotOption);
  parser.addOption(exportOption);
  parser.addOption(delayOption);
  parser.addOption(msaaOption);
  parser.addOption(fxaaOption);
  parser.addOption(aoOption);
  parser.addOption(statsOption);
  parser.addOption(animateOption);
  parser.addOption(animTimeOption);
  parser.addOption(scriptOption);
  parser.addOption(sceneExportOption);
  parser.addOption(sceneImportOption);
  parser.addOption(collabHostOption);
  parser.addOption(collabJoinOption);
  parser.addOption(collabTokenOption);
  parser.process(app);

  MainWindow window;
  window.show();

  const bool hasModel = parser.isSet(modelOption) || parser.value(surfaceOption).toInt() >= 0;

  // 与模型无关的选项可立即应用
  if (parser.isSet(modeOption)) {
    bool ok = false;
    const int mode = parser.value(modeOption).toInt(&ok);
    if (ok && mode >= 0 && mode <= 11) {
      window.setRenderMode(mode);
    }
  }
  if (parser.isSet(styleOption)) {
    bool ok = false;
    const int style = parser.value(styleOption).toInt(&ok);
    if (ok && style >= 0 && style <= 3) {
      window.applyDisplayStyle(style);
    }
  }
  if (parser.isSet(multiViewOption)) {
    window.setMultiView(true);
  }
  if (parser.isSet(axisOption)) {
    window.setAxisVisible(true);
  }
  if (parser.isSet(gridOption)) {
    window.setGridVisible(true);
  }
  if (parser.isSet(toolOption)) {
    bool ok = false;
    const int tool = parser.value(toolOption).toInt(&ok);
    if (ok && tool >= 0 && tool <= 3) {
      window.setToolMode(tool);
    }
  }
  if (parser.isSet(msaaOption)) {
    window.setMsaa(parser.value(msaaOption).toInt());
  }
  if (parser.isSet(fxaaOption)) {
    window.setFxaaEnabled(true);
  }
  if (parser.isSet(aoOption)) {
    window.setAoEnabled(true);
  }
  if (parser.isSet(collabHostOption)) {
    bool ok = false;
    const int port = parser.value(collabHostOption).toInt(&ok);
    if (ok && port > 0 && port <= 65535) {
      window.hostCollabSession(quint16(port),
                               parser.value(collabTokenOption));
    }
  }
  if (parser.isSet(collabJoinOption)) {
    const QString address = parser.value(collabJoinOption);
    const int separator = address.lastIndexOf(':');
    if (separator > 0) {
      const QString host = address.left(separator);
      const quint16 port = quint16(address.mid(separator + 1).toUInt());
      if (port != 0) {
        window.joinCollabSession(host, port,
                                 parser.value(collabTokenOption));
      }
    }
  }

  // 需要模型/曲面加载完成后再执行
  auto applySceneOptions = [&window, &parser, &viewOption, &selectOption,
                            &transformOption, &colorOption, &animateOption,
                            &animTimeOption, &scriptOption,
                            &sceneExportOption, &sceneImportOption]() {
    if (parser.isSet(viewOption)) {
      bool ok = false;
      const int preset = parser.value(viewOption).toInt(&ok);
      if (ok && preset >= 0 && preset <= 8) {
        window.applyViewPreset(preset);
      }
    }

    int selectedIndex = 0;
    if (parser.isSet(selectOption)) {
      selectedIndex = parser.value(selectOption).toInt();
      window.selectMesh(selectedIndex);
    }

    if (parser.isSet(transformOption)) {
      const QStringList parts = parser.value(transformOption).split(',');
      if (parts.size() == 9) {
        QVector3D translate(parts[0].toFloat(), parts[1].toFloat(), parts[2].toFloat());
        QVector3D rotate(parts[3].toFloat(), parts[4].toFloat(), parts[5].toFloat());
        QVector3D scale(parts[6].toFloat(), parts[7].toFloat(), parts[8].toFloat());
        window.setMeshTransformValues(qMax(0, selectedIndex),
                                      translate, rotate, scale);
      }
    }

    if (parser.isSet(colorOption)) {
      const QStringList parts = parser.value(colorOption).split(',');
      if (parts.size() == 3) {
        window.setMeshColor(qMax(0, selectedIndex),
                            QVector3D(parts[0].toFloat(),
                                      parts[1].toFloat(),
                                      parts[2].toFloat()));
      }
    }

    if (parser.isSet(animateOption)) {
      window.playAnimation();
    }
    if (parser.isSet(animTimeOption)) {
      const float seconds = parser.value(animTimeOption).toFloat();
      if (seconds >= 0.0f) {
        window.setAnimationTime(seconds);
      }
    }

    if (parser.isSet(sceneImportOption)) {
      window.importSceneState(parser.value(sceneImportOption));
    }
    if (parser.isSet(scriptOption)) {
      window.runScriptFile(parser.value(scriptOption));
    }
    if (parser.isSet(sceneExportOption)) {
      window.exportSceneState(parser.value(sceneExportOption));
    }
  };

  auto captureAndQuit = [&window, &parser, &screenshotOption, &statsOption]() {
    if (parser.isSet(statsOption)) {
      std::printf("RENDER_STATS: %s\n", window.renderStatsText().toUtf8().constData());
      std::fflush(stdout);
    }
    window.saveScreenshotTo(parser.value(screenshotOption));
    QTimer::singleShot(150, qApp, &QApplication::quit);
  };

  auto exportAndQuit = [&window, &parser, &exportOption]() {
    window.exportModelTo(parser.value(exportOption));
    QTimer::singleShot(150, qApp, &QApplication::quit);
  };

  if (hasModel) {
    QObject::connect(&window, &MainWindow::modelLoaded, &window,
                     [applySceneOptions]() { applySceneOptions(); });
    if (parser.isSet(screenshotOption)) {
      QObject::connect(
          &window, &MainWindow::modelLoaded, &window,
          [captureAndQuit, &delayOption, &parser]() {
            const int delay = parser.isSet(delayOption)
                                  ? qMax(100, parser.value(delayOption).toInt())
                                  : 600;
            QTimer::singleShot(delay, qApp,
                               [captureAndQuit]() { captureAndQuit(); });
          });
    }
    if (parser.isSet(exportOption)) {
      QObject::connect(&window, &MainWindow::modelLoaded, &window,
                       [exportAndQuit]() { exportAndQuit(); });
    }
  } else {
    applySceneOptions();
    if (parser.isSet(exportOption)) {
      QTimer::singleShot(0, qApp, [exportAndQuit]() { exportAndQuit(); });
    }
  }

  if (parser.isSet(screenshotOption) && !hasModel) {
    const int delay = qMax(100, parser.value(delayOption).toInt());
    QTimer::singleShot(delay, qApp, [captureAndQuit]() { captureAndQuit(); });
  }

  // 安全兜底：避免加载失败导致进程悬挂
  if (parser.isSet(screenshotOption) || parser.isSet(exportOption)) {
    QTimer::singleShot(30000, qApp, &QApplication::quit);
  }

  const int surface = parser.value(surfaceOption).toInt();
  if (surface >= 0 && surface <= 4) {
    window.createSurface(surface);
  } else if (parser.isSet(modelOption)) {
    window.openModel(parser.value(modelOption));
  }

  return app.exec();
}
