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

/// @brief 移动端（Android）应用程序入口点
///
/// 与桌面端 src/main.cpp 的区别：
/// - 使用 QGuiApplication + Qt Quick（不引入 QtWidgets）；
/// - 3D 视图由 MobileRhiItem（QQuickRhiItem）承载，而非 RhiWidget；
/// - 业务状态集中在 MobileViewerBridge 单例，文件选择走 MobileFileHelper。
///
/// Android 后端约定：强制 QSG_RHI_BACKEND=opengles（对应 manifest 中
/// glEsVersion 0x00030001 = OpenGL ES 3.1），qsb 着色器包含 310 es 变体。

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

#include "mobile/MobileViewerBridge.h"
#include "mobile/MobileFileHelper.h"
#include "mobile/MobileRhiItem.h"

int main(int argc, char* argv[]) {
#if defined(Q_OS_ANDROID)
    // 移动端固定 GLES 3.1 后端；如需试验 Vulkan 可注释本行
    // （着色器需相应补 Vulkan/SPIR-V 变体）
    qputenv("QSG_RHI_BACKEND", "opengles");
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("ModelViewer");
    QGuiApplication::setOrganizationName("ModelViewerProject");
    QGuiApplication::setApplicationVersion("1.1.0");

    QQuickStyle::setStyle("Material");

    // ── C++ 对象与 QML 类型注册（URI 与 qt_add_qml_module 一致）──────
    auto* bridge = new MobileViewerBridge();
    auto* fileHelper = new MobileFileHelper(bridge);

    qmlRegisterSingletonInstance("ModelViewer", 1, 0, "ViewerBridge", bridge);
    qmlRegisterSingletonInstance("ModelViewer", 1, 0, "FileHelper", fileHelper);
    qmlRegisterType<MobileRhiItem>("ModelViewer", 1, 0, "RhiView");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(url);

    // 说明：单例对象不设 parent（qmlRegisterSingletonInstance 不接管所有权）；
    // 进程退出时由操作系统回收，避免与 engine 析构顺序竞争。

    return app.exec();
}
