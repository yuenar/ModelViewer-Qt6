# ModelViewer-Qt6 移动端扩展文档

> 基于 Qt6 RHI 的 Android / iOS 3D 模型查看器移动端适配方案

---

## 目录

- [一、架构总览](#一架构总览)
- [二、移动端工程配置](#二移动端工程配置)
- [三、移动端 UI 层设计](#三移动端-ui-层设计)
- [四、触控交互系统](#四触控交互系统)
- [五、模型文件管理](#五模型文件管理)
- [六、渲染后端适配](#六渲染后端适配)
- [七、性能优化策略](#七性能优化策略)
- [八、平台原生集成](#八平台原生集成)
- [九、CMake 构建配置](#九-cmake-构建配置)

---

## 一、架构总览

### 1.1 移动端分层架构

原桌面版以 `MainWindow` + `RhiWidget` 作为 UI 宿主，移动端将 UI 层替换为 QML，核心渲染层(`RhiRenderer` / `RhiMesh` / `ModelLoader`)保持不变。

```
┌──────────────────────────────────────────┐
│           QML 移动端 UI 层                │
│  MobileViewer.qml  /  ToolBar.qml        │
│  GestureArea.qml   /  ModelListPage.qml  │
├──────────────────────────────────────────┤
│           C++ 桥接层                      │
│  MobileViewerBridge (QObject)            │
│  MobileGestureHandler                   │
│  MobileFileHelper                        │
├──────────────────────────────────────────┤
│           核心渲染层（原有，复用）          │
│  RhiRenderer   RhiMesh   ModelLoader     │
│  Camera        TrackBall BoundingBox     │
├──────────────────────────────────────────┤
│           Qt6 RHI 抽象层                  │
│  Android → OpenGL ES 3.1                 │
│  iOS     → Metal                         │
└──────────────────────────────────────────┘
```

### 1.2 新增源文件列表

| 文件路径 | 说明 |
|---|---|
| `src/mobile/MobileViewerBridge.h/.cpp` | QML ↔ C++ 主桥接对象 |
| `src/mobile/MobileGestureHandler.h/.cpp` | 触控手势 → Camera/TrackBall 转换 |
| `src/mobile/MobileFileHelper.h/.cpp` | Android SAF / iOS UIDocumentPicker 封装 |
| `src/mobile/MobileRhiItem.h/.cpp` | `QQuickRhiItem` 宿主，替代 `RhiWidget` |
| `qml/MobileViewer.qml` | 主界面根组件 |
| `qml/ViewerToolBar.qml` | 顶部工具栏 |
| `qml/GestureArea.qml` | 手势捕获覆盖层 |
| `qml/ModelListPage.qml` | 模型浏览/选择页 |
| `qml/RenderModePanel.qml` | 渲染模式侧滑面板 |
| `android/AndroidManifest.xml` | Android 权限与入口配置 |
| `ios/Info.plist` | iOS 权限与文件类型声明 |

---

## 二、移动端工程配置

### 2.1 CMakeLists.txt 移动端节点

在根 `CMakeLists.txt` 末尾添加移动端条件块：

```cmake
cmake_minimum_required(VERSION 3.20)
project(ModelViewer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---------- 公共依赖 ----------
find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core Widgets OpenGL Gui Quick QuickControls2 ShaderTools
)
find_package(assimp REQUIRED)

# ---------- 公共源文件 ----------
set(COMMON_SOURCES
    src/core/CpuMesh.h
    src/core/Light.h
    src/core/Material.h
    src/loader/ModelLoader.h
    src/loader/ModelLoader.cpp
    src/math/BoundingBox.h
    src/math/Camera.h
    src/math/Camera.cpp
    src/math/TrackBall.h
    src/math/TrackBall.cpp
    src/renderer/RhiMesh.h
    src/renderer/RhiMesh.cpp
    src/renderer/RhiRenderer.h
    src/renderer/RhiRenderer.cpp
)

# ---------- 移动端条件编译 ----------
if(ANDROID OR IOS)
    qt_add_executable(ModelViewer
        ${COMMON_SOURCES}
        src/mobile/MobileViewerBridge.h
        src/mobile/MobileViewerBridge.cpp
        src/mobile/MobileGestureHandler.h
        src/mobile/MobileGestureHandler.cpp
        src/mobile/MobileFileHelper.h
        src/mobile/MobileFileHelper.cpp
        src/mobile/MobileRhiItem.h
        src/mobile/MobileRhiItem.cpp
        src/main_mobile.cpp
    )

    qt_add_qml_module(ModelViewer
        URI "ModelViewer"
        VERSION 1.0
        QML_FILES
            qml/MobileViewer.qml
            qml/ViewerToolBar.qml
            qml/GestureArea.qml
            qml/ModelListPage.qml
            qml/RenderModePanel.qml
        RESOURCES
            res/icons/mobile/
    )

    # Android 专属
    if(ANDROID)
        set_target_properties(ModelViewer PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_MIN_SDK_VERSION 26
        )
        target_compile_definitions(ModelViewer PRIVATE PLATFORM_ANDROID)
    endif()

    # iOS 专属
    if(IOS)
        set_target_properties(ModelViewer PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
        )
        target_compile_definitions(ModelViewer PRIVATE PLATFORM_IOS)
    endif()

else()
    # ---------- 桌面端原有构建（保持不变）----------
    qt_add_executable(ModelViewer
        ${COMMON_SOURCES}
        src/ui/MainWindow.h
        src/ui/MainWindow.cpp
        src/ui/RhiWidget.h
        src/ui/RhiWidget.cpp
        src/main.cpp
    )
endif()

target_link_libraries(ModelViewer PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Quick Qt6::QuickControls2
    assimp::assimp
)
```

### 2.2 Android 构建环境

```bash
# 安装 Qt for Android（在 Qt Maintenance Tool 中勾选 Android ARM64-v8a）
# 配置 NDK 和 SDK 路径
export ANDROID_SDK_ROOT=$HOME/Android/Sdk
export ANDROID_NDK_ROOT=$HOME/Android/Sdk/ndk/26.1.10909125

# 使用 Qt Creator 构建，或命令行：
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_PREFIX_PATH=$QT6_ANDROID_PATH \
  -DASSIMP_BUILD_TESTS=OFF \
  -S .

cmake --build build-android --target apk
```

### 2.3 iOS 构建环境

```bash
# 需要 Xcode 15+ 和 Qt for iOS
cmake -B build-ios \
  -GXcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_PREFIX_PATH=$QT6_IOS_PATH \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DASSIMP_BUILD_TESTS=OFF \
  -S .

# 使用 Xcode 打开并签名
open build-ios/ModelViewer.xcodeproj
```

---

## 三、移动端 UI 层设计

### 3.1 主入口 `src/main_mobile.cpp`

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "mobile/MobileViewerBridge.h"
#include "mobile/MobileFileHelper.h"

int main(int argc, char *argv[])
{
    // 移动端必须在 QGuiApplication 创建前设置 RHI 后端
#if defined(PLATFORM_ANDROID)
    qputenv("QSG_RHI_BACKEND", "opengles");
#elif defined(PLATFORM_IOS)
    qputenv("QSG_RHI_BACKEND", "metal");
#endif

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");

    // 注册 QML 类型
    qmlRegisterType<MobileViewerBridge>("ModelViewer", 1, 0, "ViewerBridge");
    qmlRegisterSingletonType<MobileFileHelper>("ModelViewer", 1, 0, "FileHelper",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            return new MobileFileHelper();
        });

    QQmlApplicationEngine engine;
    engine.load(QUrl(u"qrc:/ModelViewer/qml/MobileViewer.qml"_qs));
    return app.exec();
}
```

### 3.2 QML 主界面 `qml/MobileViewer.qml`

```qml
import QtQuick
import QtQuick.Controls.Material
import ModelViewer

ApplicationWindow {
    id: root
    visible: true
    width: Screen.width
    height: Screen.height
    title: "ModelViewer"

    // C++ 桥接实例，持有 RhiRenderer
    ViewerBridge {
        id: bridge
        onModelLoaded: (modelName) => toolBar.title = modelName
        onLoadError:   (msg)       => errorBanner.show(msg)
    }

    // 顶部工具栏
    ViewerToolBar {
        id: toolBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        onOpenFileClicked:        FileHelper.pickModel()
        onRenderPanelToggled:     renderPanel.open()
        onResetViewClicked:       bridge.resetView()
        onScreenshotClicked:      bridge.saveScreenshot()
        onProjectionToggled: (ortho) => bridge.setOrthographic(ortho)
    }

    // 3D 渲染区域（MobileRhiItem 内嵌 RhiRenderer）
    MobileRhiItemView {
        id: rhiView
        anchors {
            top: toolBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        bridge: bridge

        // 手势覆盖层
        GestureArea {
            anchors.fill: parent
            onRotate:  (dx, dy)     => bridge.rotate(dx, dy)
            onPan:     (dx, dy)     => bridge.pan(dx, dy)
            onZoom:    (scale)      => bridge.zoom(scale)
            onDoubleTap:            bridge.fitView()
            onLongPress:(x, y)      => bridge.pickObject(x, y)
        }
    }

    // 渲染模式侧滑面板
    RenderModePanel {
        id: renderPanel
        width: Math.min(parent.width * 0.75, 320)
        height: parent.height
        onModeSelected: (mode) => bridge.setRenderMode(mode)
    }

    // 错误横幅
    BannerMessage { id: errorBanner; anchors.bottom: parent.bottom }

    // 监听文件选择结果
    Connections {
        target: FileHelper
        function onFileSelected(path) { bridge.loadModel(path) }
    }
}
```

### 3.3 工具栏 `qml/ViewerToolBar.qml`

```qml
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

ToolBar {
    id: bar
    property string title: "ModelViewer"

    signal openFileClicked
    signal renderPanelToggled
    signal resetViewClicked
    signal screenshotClicked
    signal projectionToggled(bool ortho)

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ToolButton {
            icon.source: "qrc:/icons/folder_open.svg"
            onClicked: bar.openFileClicked()
            ToolTip.text: "打开模型"
            ToolTip.visible: hovered
        }

        Label {
            Layout.fillWidth: true
            text: bar.title
            font.pixelSize: 16
            font.bold: true
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
        }

        ToolButton {
            icon.source: "qrc:/icons/layers.svg"
            onClicked: bar.renderPanelToggled()
            ToolTip.text: "渲染模式"
        }

        ToolButton {
            icon.source: "qrc:/icons/fit_screen.svg"
            onClicked: bar.resetViewClicked()
            ToolTip.text: "重置视图"
        }

        ToolButton {
            id: projBtn
            checkable: true
            icon.source: checked ? "qrc:/icons/ortho.svg" : "qrc:/icons/perspective.svg"
            onCheckedChanged: bar.projectionToggled(checked)
            ToolTip.text: checked ? "正交投影" : "透视投影"
        }

        ToolButton {
            icon.source: "qrc:/icons/camera.svg"
            onClicked: bar.screenshotClicked()
            ToolTip.text: "截图"
        }
    }
}
```

### 3.4 渲染模式面板 `qml/RenderModePanel.qml`

```qml
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Drawer {
    id: panel
    edge: Qt.RightEdge
    signal modeSelected(int mode)

    // 渲染模式枚举与 RhiRenderer::RenderMode 对齐
    readonly property var modes: [
        { name: "Phong 着色",    icon: "qrc:/icons/phong.svg",     value: 0 },
        { name: "线框模式",      icon: "qrc:/icons/wireframe.svg", value: 1 },
        { name: "法线可视化",    icon: "qrc:/icons/normal.svg",    value: 2 },
        { name: "平面着色",      icon: "qrc:/icons/flat.svg",      value: 3 },
        { name: "PBR 渲染",      icon: "qrc:/icons/pbr.svg",       value: 4 },
        { name: "阴影映射",      icon: "qrc:/icons/shadow.svg",    value: 5 },
        { name: "天空盒",        icon: "qrc:/icons/skybox.svg",    value: 6 },
        { name: "剖面视图",      icon: "qrc:/icons/clip.svg",      value: 7 },
        { name: "分屏对比",      icon: "qrc:/icons/splitview.svg", value: 8 },
    ]

    ListView {
        anchors.fill: parent
        model: panel.modes
        delegate: ItemDelegate {
            width: parent.width
            contentItem: RowLayout {
                Image { source: modelData.icon; width: 24; height: 24 }
                Label { text: modelData.name; font.pixelSize: 15 }
            }
            onClicked: {
                panel.modeSelected(modelData.value)
                panel.close()
            }
        }
    }
}
```

---

## 四、触控交互系统

### 4.1 手势捕获 `qml/GestureArea.qml`

```qml
import QtQuick

Item {
    id: root

    // 信号定义，由 C++ 手势层或 QML 直接消费
    signal rotate(real dx, real dy)
    signal pan(real dx, real dy)
    signal zoom(real scaleDelta)
    signal doubleTap()
    signal longPress(real x, real y)

    // 单指旋转
    DragHandler {
        id: rotateDrag
        acceptedButtons: Qt.NoButton   // 触屏无按键
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Mouse
        onActiveChanged: if (!active) _lastPos = Qt.point(0, 0)
        onCentroidChanged: {
            if (_lastPos.x !== 0) {
                root.rotate(centroid.position.x - _lastPos.x,
                            centroid.position.y - _lastPos.y)
            }
            _lastPos = centroid.position
        }
        property point _lastPos: Qt.point(0, 0)
    }

    // 双指缩放
    PinchHandler {
        id: pinch
        acceptedDevices: PointerDevice.TouchScreen
        onScaleChanged: root.zoom(activeScale - 1.0)
        onTranslationChanged: root.pan(translation.x, translation.y)
    }

    // 双击复位
    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Mouse
        numberOfTapsRequired: 2
        onTapped: root.doubleTap()
    }

    // 长按拾取
    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen
        longPressThreshold: 0.6
        onLongPressed: root.longPress(point.position.x, point.position.y)
    }
}
```

### 4.2 手势处理器 `src/mobile/MobileGestureHandler.h`

```cpp
#pragma once
#include <QObject>
#include "math/Camera.h"
#include "math/TrackBall.h"

// 将 QML 手势信号转换为 Camera / TrackBall 操作
class MobileGestureHandler : public QObject
{
    Q_OBJECT
public:
    explicit MobileGestureHandler(Camera* camera,
                                  TrackBall* trackBall,
                                  QObject* parent = nullptr);

    // 由 MobileViewerBridge 转发 QML 信号
    void handleRotate(float dx, float dy);
    void handlePan(float dx, float dy);
    void handleZoom(float scaleDelta);
    void handleFitView(const BoundingBox& bounds);

private:
    Camera*     m_camera;
    TrackBall*  m_trackBall;

    // 触控灵敏度系数（移动端屏幕密度适配）
    static constexpr float kRotateSensitivity = 0.5f;
    static constexpr float kPanSensitivity    = 0.003f;
    static constexpr float kZoomSensitivity   = 1.5f;
};
```

### 4.3 手势处理器实现 `src/mobile/MobileGestureHandler.cpp`

```cpp
#include "MobileGestureHandler.h"
#include <QScreen>
#include <QGuiApplication>
#include <cmath>

MobileGestureHandler::MobileGestureHandler(Camera* camera,
                                           TrackBall* trackBall,
                                           QObject* parent)
    : QObject(parent), m_camera(camera), m_trackBall(trackBall)
{}

void MobileGestureHandler::handleRotate(float dx, float dy)
{
    // 将像素偏移转换为角度，乘以屏幕 DPI 系数归一化
    const float dpi = QGuiApplication::primaryScreen()->devicePixelRatio();
    m_trackBall->rotate(dx * kRotateSensitivity / dpi,
                        dy * kRotateSensitivity / dpi);
}

void MobileGestureHandler::handlePan(float dx, float dy)
{
    const float dpi = QGuiApplication::primaryScreen()->devicePixelRatio();
    m_camera->pan(dx * kPanSensitivity / dpi,
                  dy * kPanSensitivity / dpi);
}

void MobileGestureHandler::handleZoom(float scaleDelta)
{
    // scaleDelta > 0 放大，< 0 缩小
    m_camera->zoom(-scaleDelta * kZoomSensitivity);
}

void MobileGestureHandler::handleFitView(const BoundingBox& bounds)
{
    m_camera->fitBounds(bounds);
    m_trackBall->reset();
}
```

---

## 五、模型文件管理

### 5.1 平台文件选择器 `src/mobile/MobileFileHelper.h`

```cpp
#pragma once
#include <QObject>
#include <QString>

// 跨平台文件选择器
// Android 使用 Qt Android Extras / JNI 调用 SAF
// iOS 使用 Qt iOS Extras / NSDocumentPickerViewController
class MobileFileHelper : public QObject
{
    Q_OBJECT
public:
    explicit MobileFileHelper(QObject* parent = nullptr);

    Q_INVOKABLE void pickModel();       // QML 调用：弹出系统文件选择器
    Q_INVOKABLE QStringList recentModels() const;  // 最近打开列表
    Q_INVOKABLE void clearRecents();

signals:
    void fileSelected(const QString& path);
    void pickCancelled();

private:
    void saveToRecents(const QString& path);
    QStringList m_recents;

    static const int kMaxRecents = 10;
};
```

### 5.2 Android 实现片段

```cpp
// MobileFileHelper_android.cpp
#ifdef PLATFORM_ANDROID
#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>

void MobileFileHelper::pickModel()
{
    // 支持格式 MIME 类型
    const QStringList mimeTypes = {
        "model/obj", "model/gltf+json", "model/gltf-binary",
        "application/octet-stream"   // FBX / 3DS / STL 通用
    };

    auto intent = QJniObject("android/content/Intent",
                             "(Ljava/lang/String;)V",
                             QJniObject::fromString("android.intent.action.OPEN_DOCUMENT").object());
    intent.callMethod<void>("addCategory", "(Ljava/lang/String;)V",
        QJniObject::fromString("android.intent.category.OPENABLE").object());
    intent.callMethod<void>("setType", "(Ljava/lang/String;)V",
        QJniObject::fromString("*/*").object());

    // 设置多 MIME 类型过滤
    QJniObject mimeArray = QJniObject::callStaticObjectMethod(
        "java/util/Arrays", "copyOf", ...);
    intent.callMethod<void>("putExtra", "(Ljava/lang/String;[Ljava/lang/String;)V",
        QJniObject::fromString("android.intent.extra.MIME_TYPES").object(),
        mimeArray.object());

    QtAndroidPrivate::startActivity(intent, 42, [this](int, int resultCode, const QJniObject& data) {
        if (resultCode == -1 && data.isValid()) {  // RESULT_OK
            const QString path = data.callObjectMethod("getData", "()Landroid/net/Uri;")
                                     .callObjectMethod("toString", "()Ljava/lang/String;")
                                     .toString();
            emit fileSelected(path);
            saveToRecents(path);
        } else {
            emit pickCancelled();
        }
    });
}
#endif
```

### 5.3 iOS 实现片段

```cpp
// MobileFileHelper_ios.mm
#ifdef PLATFORM_IOS
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

void MobileFileHelper::pickModel()
{
    NSArray<UTType*>* types = @[
        [UTType typeWithFilenameExtension:@"obj"],
        [UTType typeWithFilenameExtension:@"gltf"],
        [UTType typeWithFilenameExtension:@"glb"],
        [UTType typeWithFilenameExtension:@"fbx"],
        [UTType typeWithFilenameExtension:@"stl"],
        [UTType typeWithFilenameExtension:@"3ds"],
        [UTType typeWithFilenameExtension:@"dae"],
    ];

    UIDocumentPickerViewController* picker =
        [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types];
    picker.allowsMultipleSelection = NO;

    // 通过 delegate 回调
    // __bridge 转回 C++ this 指针
    MobileFilePickerDelegate* delegate =
        [[MobileFilePickerDelegate alloc] initWithCallback:^(NSURL* url) {
            const QString path = QString::fromNSString(url.absoluteString);
            emit fileSelected(path);
            saveToRecents(path);
        }];
    picker.delegate = delegate;

    UIViewController* rootVC =
        [UIApplication sharedApplication].keyWindow.rootViewController;
    [rootVC presentViewController:picker animated:YES completion:nil];
}
#endif
```

### 5.4 模型列表页 `qml/ModelListPage.qml`

```qml
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import ModelViewer

Page {
    title: "选择模型"

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.source: "qrc:/icons/arrow_back.svg"
                onClicked: pageStack.pop()
            }
            Label {
                text: "模型库"
                font.pixelSize: 18
                font.bold: true
                Layout.fillWidth: true
            }
            ToolButton {
                icon.source: "qrc:/icons/add.svg"
                onClicked: FileHelper.pickModel()
            }
        }
    }

    // 内置演示模型 + 最近打开
    ListView {
        anchors.fill: parent
        model: ListModel {
            id: modelList
            Component.onCompleted: {
                // 内置示例模型
                append({ name: "茶壶",       path: "qrc:/models/teapot.obj",   thumb: "qrc:/thumbnails/teapot.png"  })
                append({ name: "Stanford 兔", path: "qrc:/models/bunny.obj",    thumb: "qrc:/thumbnails/bunny.png"   })
                append({ name: "龙模型",      path: "qrc:/models/dragon.obj",   thumb: "qrc:/thumbnails/dragon.png"  })
                append({ name: "宇航员",      path: "qrc:/models/astronaut.glb",thumb: "qrc:/thumbnails/astro.png"   })
                // 最近打开
                const recents = FileHelper.recentModels()
                for (let p of recents) {
                    append({ name: p.split("/").pop(), path: p, thumb: "qrc:/icons/file_3d.svg" })
                }
            }
        }

        delegate: ItemDelegate {
            width: parent.width
            height: 72
            contentItem: RowLayout {
                spacing: 12
                Image {
                    source: model.thumb
                    width: 48; height: 48
                    fillMode: Image.PreserveAspectFit
                }
                Column {
                    Label { text: model.name; font.pixelSize: 15; font.bold: true }
                    Label { text: model.path; font.pixelSize: 11; opacity: 0.6; elide: Text.ElideLeft }
                }
            }
            onClicked: {
                bridge.loadModel(model.path)
                pageStack.pop()
            }
        }
    }
}
```

---

## 六、渲染后端适配

### 6.1 `MobileRhiItem` — `QQuickRhiItem` 宿主

```cpp
// src/mobile/MobileRhiItem.h
#pragma once
#include <QtQuick/QQuickRhiItem>
#include "renderer/RhiRenderer.h"
#include "mobile/MobileViewerBridge.h"

// 作为 QML 中的渲染控件，替代桌面端的 RhiWidget
class MobileRhiItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(MobileViewerBridge* bridge READ bridge WRITE setBridge NOTIFY bridgeChanged)

public:
    explicit MobileRhiItem(QQuickItem* parent = nullptr);

    MobileViewerBridge* bridge() const { return m_bridge; }
    void setBridge(MobileViewerBridge* b);

    QQuickRhiItemRenderer* createRenderer() override;

signals:
    void bridgeChanged();

private:
    MobileViewerBridge* m_bridge = nullptr;
};

// --- Renderer ---
class MobileRhiItemRenderer : public QQuickRhiItemRenderer
{
public:
    explicit MobileRhiItemRenderer(RhiRenderer* renderer);
    void initialize(QRhi* rhi, QRhiRenderTarget* renderTarget,
                    QRhiCommandBuffer* cb, const QSize& size) override;
    void synchronize(QQuickRhiItem* item) override;
    void render(QRhiCommandBuffer* cb) override;

private:
    RhiRenderer* m_renderer;
};
```

### 6.2 Android OpenGL ES 3.1 着色器兼容

Android 不支持 OpenGL ES < 3.1 的几何着色器，在 `CMakeLists.txt` 中追加编译定义：

```cmake
if(ANDROID)
    target_compile_definitions(ModelViewer PRIVATE
        NOGEOMSHADER          # 禁用几何着色器渲染路径
        QT_RHI_GLES_COMPAT    # 强制 GLSL 版本为 #version 310 es
    )
endif()
```

在 `RhiRenderer.cpp` 对应位置加守卫：

```cpp
void RhiRenderer::buildNormalPipeline()
{
#ifdef NOGEOMSHADER
    // 移动端：用顶点着色器模拟法线线段（双顶点 drawArrays）
    m_normalPipeline = buildPipelineFromFiles(
        ":/shaders/normal_mobile.vert",
        ":/shaders/normal_mobile.frag");
#else
    m_normalPipeline = buildPipelineFromFiles(
        ":/shaders/normal.vert",
        ":/shaders/normal.geom",
        ":/shaders/normal.frag");
#endif
}
```

### 6.3 iOS Metal 后端要求

iOS 使用 Metal 后端，需确保：

- 所有 GLSL 着色器通过 `qt_add_shaders()` 转译为 MSL（Qt Shader Tools 自动处理）
- `clipSpaceCorrMatrix()` 已在 `RhiRenderer` 中正确调用（Metal NDC 与 OpenGL 不同）
- 纹理坐标翻转通过 Uniform Buffer 中的 `uFlipY` 标志控制

```cmake
# CMakeLists.txt：着色器编译
qt_add_shaders(ModelViewer "model_shaders"
    GLSL "310 es,410"
    HLSL 50
    MSL 12
    FILES
        shaders/phong.vert
        shaders/phong.frag
        shaders/pbr.vert
        shaders/pbr.frag
        shaders/wireframe.vert
        shaders/wireframe.frag
        shaders/shadow.vert
        shaders/shadow.frag
        shaders/skybox.vert
        shaders/skybox.frag
        shaders/normal_mobile.vert
        shaders/normal_mobile.frag
)
```

---

## 七、性能优化策略

### 7.1 移动端 LOD 策略

移动端 GPU 算力有限，对超过阈值面数的模型自动降级：

```cpp
// src/loader/ModelLoader.cpp 新增移动端预处理
#ifdef Q_OS_ANDROID
    // Android：超过 200K 三角面时启用 Assimp 简化
    constexpr unsigned int kMobileMaxTris = 200'000;
    if (totalTris > kMobileMaxTris) {
        postProcessFlags |= aiProcess_OptimizeMeshes
                         |  aiProcess_OptimizeGraph;
        // SimplifyMesh 参数：保留 50% 面数
        ai_real simplifyFactor = float(kMobileMaxTris) / totalTris * 2.0f;
        importer.SetPropertyFloat(AI_CONFIG_PP_LBW_MAX_WEIGHTS, simplifyFactor);
    }
#endif
```

### 7.2 纹理压缩

| 平台 | 格式 | 说明 |
|---|---|---|
| Android (OpenGL ES) | ETC2 / ASTC | 硬件解码，运行时零额外内存 |
| iOS (Metal) | ASTC 4×4 | 所有 A8 以上 SoC 均支持 |

```cpp
// src/renderer/RhiRenderer.cpp — 移动端纹理上传
QRhiTexture::Format pickTextureFormat(bool hasAlpha)
{
#if defined(PLATFORM_ANDROID)
    return hasAlpha ? QRhiTexture::ETC2_RGBA8 : QRhiTexture::ETC2_RGB8;
#elif defined(PLATFORM_IOS)
    return hasAlpha ? QRhiTexture::ASTC_4x4 : QRhiTexture::ASTC_4x4;
#else
    return QRhiTexture::RGBA8;
#endif
}
```

### 7.3 帧率控制

移动端默认 60fps，当电量 ≤ 20% 时降至 30fps：

```cpp
// src/mobile/MobileViewerBridge.cpp
void MobileViewerBridge::onBatteryLevelChanged(int level)
{
    const int fps = (level <= 20) ? 30 : 60;
    m_rhiItem->setUpdateBehavior(fps == 30
        ? QQuickRhiItem::NoExplicitSync
        : QQuickRhiItem::UseExplicitSync);
    // 通知 QQuickWindow 刷新节流
    if (auto* win = m_rhiItem->window())
        win->setFramesPerSecond(fps);
}
```

### 7.4 离屏时暂停渲染

```cpp
// 监听 Qt::ApplicationState 减少后台 GPU 占用
connect(qApp, &QGuiApplication::applicationStateChanged,
        this, [this](Qt::ApplicationState state) {
    m_renderPaused = (state != Qt::ApplicationActive);
});

void MobileRhiItemRenderer::render(QRhiCommandBuffer* cb)
{
    if (m_bridge && m_bridge->isRenderPaused()) return;
    // ... 正常渲染
}
```

---

## 八、平台原生集成

### 8.1 Android Manifest

```xml
<!-- android/AndroidManifest.xml -->
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.modelviewer">

    <!-- 读取外部存储（Android 12 以下）-->
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE"
        android:maxSdkVersion="32" />

    <!-- Android 13+ 细分权限 -->
    <uses-permission android:name="android.permission.READ_MEDIA_IMAGES" />

    <!-- OpenGL ES 3.1 要求 -->
    <uses-feature android:glEsVersion="0x00030001" android:required="true" />
    <uses-feature android:name="android.hardware.vulkan.level" android:required="false" />

    <application
        android:label="ModelViewer"
        android:icon="@drawable/icon"
        android:hardwareAccelerated="true">

        <activity
            android:name="org.qtproject.qt.android.bindings.QtActivity"
            android:exported="true"
            android:screenOrientation="fullSensor"
            android:configChanges="orientation|screenSize|keyboardHidden">

            <!-- 声明可接收的 3D 文件 MIME 类型 -->
            <intent-filter>
                <action android:name="android.intent.action.VIEW" />
                <category android:name="android.intent.category.DEFAULT" />
                <data android:mimeType="model/obj" />
                <data android:mimeType="model/gltf+json" />
                <data android:mimeType="model/gltf-binary" />
                <data android:mimeType="application/octet-stream" />
            </intent-filter>
        </activity>
    </application>
</manifest>
```

### 8.2 iOS Info.plist

```xml
<!-- ios/Info.plist（关键条目）-->
<key>CFBundleDocumentTypes</key>
<array>
    <dict>
        <key>CFBundleTypeName</key><string>3D Model</string>
        <key>LSItemContentTypes</key>
        <array>
            <string>public.geometry-definition-format</string> <!-- OBJ -->
            <string>com.khronos.gltf</string>
            <string>com.autodesk.fbx</string>
            <string>org.stereolithography.stl</string>
        </array>
        <key>CFBundleTypeRole</key><string>Viewer</string>
    </dict>
</array>

<!-- 访问 Files App 中的文件 -->
<key>UIFileSharingEnabled</key><true/>
<key>LSSupportsOpeningDocumentsInPlace</key><true/>

<!-- 支持横竖屏 -->
<key>UISupportedInterfaceOrientations</key>
<array>
    <string>UIInterfaceOrientationPortrait</string>
    <string>UIInterfaceOrientationLandscapeLeft</string>
    <string>UIInterfaceOrientationLandscapeRight</string>
</array>

<!-- Metal GPU 要求 -->
<key>UIRequiredDeviceCapabilities</key>
<array><string>metal</string></array>
```

### 8.3 `MobileViewerBridge` 核心接口

```cpp
// src/mobile/MobileViewerBridge.h
#pragma once
#include <QObject>
#include <QVariantMap>
#include "renderer/RhiRenderer.h"
#include "mobile/MobileGestureHandler.h"

class MobileViewerBridge : public QObject
{
    Q_OBJECT
public:
    explicit MobileViewerBridge(QObject* parent = nullptr);

    bool isRenderPaused() const { return m_renderPaused; }

public slots:
    // QML 调用
    void loadModel(const QString& path);
    void setRenderMode(int mode);           // RhiRenderer::RenderMode
    void setOrthographic(bool ortho);
    void resetView();
    void fitView();
    void rotate(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float scaleDelta);
    void pickObject(float x, float y);
    void saveScreenshot();

signals:
    void modelLoaded(const QString& modelName);
    void loadError(const QString& message);
    void loadProgress(float progress);      // 0.0–1.0，用于进度条
    void renderModeChanged(int mode);
    void screenshotSaved(const QString& path);

private:
    RhiRenderer*         m_renderer    = nullptr;
    MobileGestureHandler* m_gesture   = nullptr;
    bool                  m_renderPaused = false;
};
```

---

## 九、CMake 构建配置

### 9.1 Android 快速构建脚本 `scripts/build_android.sh`

```bash
#!/usr/bin/env bash
set -e

QT6_ANDROID="${QT6_ANDROID_PATH:-$HOME/Qt/6.9.3/android_arm64_v8a}"
NDK="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/26.1.10909125}"
BUILD_DIR="build-android"

cmake -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_PREFIX_PATH="$QT6_ANDROID" \
  -DASSIMP_BUILD_TESTS=OFF \
  -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF \
  -DASSIMP_BUILD_OBJ_IMPORTER=ON \
  -DASSIMP_BUILD_FBX_IMPORTER=ON \
  -DASSIMP_BUILD_GLTF_IMPORTER=ON \
  -DASSIMP_BUILD_STL_IMPORTER=ON \
  -DASSIMP_BUILD_3DS_IMPORTER=ON \
  -DASSIMP_BUILD_COLLADA_IMPORTER=ON \
  -S .

cmake --build "$BUILD_DIR" --target apk -j$(nproc)
echo "APK: $BUILD_DIR/android-build/build/outputs/apk/release/"
```

### 9.2 iOS 快速构建脚本 `scripts/build_ios.sh`

```bash
#!/usr/bin/env bash
set -e

QT6_IOS="${QT6_IOS_PATH:-$HOME/Qt/6.9.3/ios}"
BUILD_DIR="build-ios"

cmake -B "$BUILD_DIR" \
  -GXcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_PREFIX_PATH="$QT6_IOS" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DASSIMP_BUILD_TESTS=OFF \
  -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF \
  -DASSIMP_BUILD_OBJ_IMPORTER=ON \
  -DASSIMP_BUILD_FBX_IMPORTER=ON \
  -DASSIMP_BUILD_GLTF_IMPORTER=ON \
  -DASSIMP_BUILD_STL_IMPORTER=ON \
  -DASSIMP_BUILD_3DS_IMPORTER=ON \
  -DASSIMP_BUILD_COLLADA_IMPORTER=ON \
  -S .

echo "请使用 Xcode 打开 $BUILD_DIR/ModelViewer.xcodeproj 进行签名和部署"
open "$BUILD_DIR/ModelViewer.xcodeproj"
```

### 9.3 最终目录结构（增量）

```
ModelViewer-Qt6/
├── src/
│   ├── core/               # 不变
│   ├── loader/             # 新增移动端 LOD 预处理守卫
│   ├── math/               # 不变
│   ├── renderer/           # 新增 NOGEOMSHADER 守卫
│   ├── ui/                 # 桌面端，不变
│   ├── mobile/             # 新增
│   │   ├── MobileViewerBridge.h/.cpp
│   │   ├── MobileGestureHandler.h/.cpp
│   │   ├── MobileFileHelper.h/.cpp
│   │   ├── MobileFileHelper_android.cpp
│   │   ├── MobileFileHelper_ios.mm
│   │   └── MobileRhiItem.h/.cpp
│   ├── main.cpp            # 桌面端入口，不变
│   └── main_mobile.cpp     # 新增，移动端入口
├── qml/                    # 新增
│   ├── MobileViewer.qml
│   ├── ViewerToolBar.qml
│   ├── GestureArea.qml
│   ├── ModelListPage.qml
│   └── RenderModePanel.qml
├── shaders/
│   ├── normal_mobile.vert  # 新增，无几何着色器法线渲染
│   └── normal_mobile.frag
├── android/
│   └── AndroidManifest.xml # 新增
├── ios/
│   └── Info.plist          # 新增
├── scripts/
│   ├── build_android.sh    # 新增
│   └── build_ios.sh        # 新增
└── CMakeLists.txt          # 扩展，添加移动端条件块
```

---

> **兼容性说明**
> - Android：minSdk 26（Android 8.0），targetSdk 34，OpenGL ES 3.1，Vulkan 可选
> - iOS：部署目标 16.0，Metal，支持 iPhone 和 iPad（Universal）
> - 桌面端原有代码路径零改动，移动端完全通过条件编译隔离
