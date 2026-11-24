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
/// Android 后端约定：按系统 API 级别选择渲染后端——SDK_INT > 31
/// （Android 12L 及以上）使用 Vulkan，其余回退 OpenGL ES 3.1
/// （manifest glEsVersion 0x00030001；qsb 同时带 SPIR-V 与 310 es 变体）。

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QQuickView>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QScreen>
#include <QRect>
#include <QSize>
#include <QSurfaceFormat>
#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

#include "mobile/MobileViewerBridge.h"
#include "mobile/MobileFileHelper.h"
#include "mobile/MobileRhiItem.h"

int main(int argc, char* argv[]) {
#if defined(Q_OS_ANDROID)
    // ── 渲染后端：按 Android API 级别选择 ──────────────────────────
    // SDK_INT > 31（Android 12L 及以上）走 Vulkan，否则 OpenGL ES 3.1。
    // QSG_RHI_BACKEND 必须在 QGuiApplication 构造前写入（Qt Quick 初始化
    // 时读取，之后设置无效）。
    // 🔴 不要再写 qputenv("QT_OPENGL", "es2")：那是桌面 QT_OPENGL 变量的
    //    历史取值，Android 平台插件根本不读它，写在这儿只会误导后来者。
    const int sdkInt = QJniObject::getStaticField<jint>(
        "android/os/Build$VERSION", "SDK_INT");
    const bool useVulkan = sdkInt > 31;
    qputenv("QSG_RHI_BACKEND", useVulkan ? "vulkan" : "opengl");

    if (!useVulkan) {
        // GLES 路径：指定 RenderableType=OpenGLES + 版本，Android 平台插件
        // 据此创建 EGL 上下文（驱动支持更高版本时会给出 ES 3.2，这里只保证下限）。
        QSurfaceFormat esFormat;
        esFormat.setRenderableType(QSurfaceFormat::OpenGLES);
        esFormat.setVersion(3, 1);  // 自绘 QSG 节点/着色器的下限
        esFormat.setDepthBufferSize(24);
        esFormat.setStencilBufferSize(8);
        esFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        QSurfaceFormat::setDefaultFormat(esFormat);
    }

    // 帧级诊断（与桌面一致，靠环境变量开关，正式包不带）
    if (!qEnvironmentVariableIsEmpty("MYSIC_QSG_TIMING")) {
        qputenv("QSG_RENDER_TIMING", "1");
        qputenv("QSG_INFO", "1");
    }

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
