# ModelViewer-Qt6 移动端扩展文档（按当前实现整理）

> 本文不是设计方案，而是**从当前仓库实际代码反推的实现说明**。核对基准：2026-11-24。
> 范围：桌面端继续沿用 `src/main.cpp` + Qt Widgets；移动端目前**只实现了 Android**，iOS 本轮未实现（根 `CMakeLists.txt` 里只有 `if(IOS) message(...)` 占位）。

---

## 目录

- [一、现状与能力边界](#一现状与能力边界)
- [二、架构总览](#二架构总览)
- [三、工程配置与构建](#三工程配置与构建)
- [四、入口与 QML 类型注册](#四入口与-qml-类型注册)
- [五、QML UI 层](#五qml-ui-层)
- [六、触控交互](#六触控交互)
- [七、模型加载与文件管理](#七模型加载与文件管理)
- [八、渲染、截图与统计](#八渲染截图与统计)
- [九、性能与后台策略](#九性能与后台策略)
- [十、调试与故障排查](#十调试与故障排查)
- [十一、与早期设计稿的差异 / 未实现项](#十一与早期设计稿的差异--未实现项)
- [附录：实际目录结构](#附录实际目录结构)

---

## 一、现状与能力边界

### 1.1 已实现

| 能力 | 实现位置 |
|---|---|
| 独立 Android 入口（QGuiApplication + Qt Quick） | `src/main_mobile.cpp` |
| QML 主界面、工具栏、手势层、模型列表、渲染面板 | `src/qml/*.qml`（5 个文件） |
| QML ↔ C++ 桥接（场景状态、手势换算、异步加载、拾取、动画、截图请求） | `src/mobile/MobileViewerBridge.h/.cpp` |
| `QQuickRhiItem` 3D 宿主 + 渲染线程快照 | `src/mobile/MobileRhiItem.h/.cpp` |
| Android SAF 文件选择、`content://` → cache 拷贝、最近打开 | `src/mobile/MobileFileHelper_android.cpp` + `MobileFileHelper.cpp/.h` |
| 桌面 / Android 双渲染后端（按 API 级别分流） | `src/main_mobile.cpp`、`android/AndroidManifest.xml` |
| 12 种渲染模式 + 4 种显示风格 + 显示选项 | `RenderModePanel.qml` → `MobileViewerBridge` → 共用的 `RhiRenderer` |
| 三角形级 CPU 射线拾取 | `MobileViewerBridge::pickMesh` / `pickObject` |
| 异步 readback 截图（GLES 同步 / Vulkan 异步） | `MobileRhiItemRenderer::grabFrame` |
| 后台暂停渲染、FPS/绘制统计 HUD、动画播放控制 | `MobileViewerBridge` + `MobileRhiItemRenderer` + `MobileViewer.qml` |

### 1.2 不支持 / 本轮未做

iOS（无 `Info.plist`、无 `MobileFileHelper_ios.mm`）、`MobileGestureHandler`、运行时纹理压缩格式选择、加载时自动面数简化、低电量自动降帧、独立 `normal_mobile.vert/frag` 着色器。详见[第十一章](#十一与早期设计稿的差异--未实现项)。

---

## 二、架构总览

### 2.1 分层结构

```
┌──────────────────────────────────────────────┐
│ QML UI（src/qml/）                            │
│ MobileViewer.qml  ViewerToolBar.qml          │
│ GestureArea.qml   ModelListPage.qml          │
│ RenderModePanel.qml                          │
├──────────────────────────────────────────────┤
│ C++ 桥接层（src/mobile/，GUI 线程）           │
│ MobileViewerBridge（QML 单例 ViewerBridge）  │
│ MobileFileHelper（QML 单例 FileHelper）      │
│ MobileRhiItem（QML 类型 RhiView）             │
├──────────────────────────────────────────────┤
│ 复用核心层（src/core、loader、math、renderer）│
│ RhiRenderer / RhiMesh / BufferPool /          │
│ ModelLoader / ModelAsset / Camera / Light /   │
│ Material / BoundingBox / ParametricSurface    │
├──────────────────────────────────────────────┤
│ Qt RHI（main_mobile.cpp 选择后端）            │
│ Android API > 31 → Vulkan；否则 OpenGL ES 3.1 │
└──────────────────────────────────────────────┘
```

桌面专属的 `src/ui/`、`src/net/`、`src/plugin/` 在 Android 分支里根本不参与编译（`src/CMakeLists.txt` 的 `if(ANDROID)` 块收集完源码后直接 `return()`，不进入桌面段）。

### 2.2 线程模型

| 线程 | 做什么 |
|---|---|
| GUI 线程 | `MobileViewerBridge` 全部状态、QML、手势换算、模型上传前的 CPU 数据、`QTimer` 动画推进 |
| `QtConcurrent` 线程池 | `loadModel()` 中 `QtConcurrent::run` 调用 `ModelLoader::loadCPU`，`QFutureWatcher<ModelAsset>` 回 GUI 线程收结果 |
| 渲染线程 | `MobileRhiItemRenderer::initialize/synchronize/render`。`synchronize()` 是**唯一的数据同步点**，GUI 线程在此时阻塞；渲染侧只使用快照，不直接读写 Bridge 对象 |

`MobileViewerBridge` 里带 `[SG-sync]` 注释的接口就是约定给 `synchronize()` 在渲染线程调用的（读状态、一次性取标志、回收退役网格、交付截图、回写统计）；其它接口只允许 GUI 线程访问。

### 2.3 实际新增/改动文件清单

| 路径 | 说明 |
|---|---|
| `src/main_mobile.cpp` | Android 入口；RHI 后端选择、QML 类型注册、加载主 QML |
| `src/mobile/MobileViewerBridge.h/.cpp` | QML 单例 `ViewerBridge`；场景中枢 |
| `src/mobile/MobileFileHelper.h/.cpp` | QML 单例 `FileHelper` 的跨平台部分：构造、recents、非 Android 的 `pickModel()` 兜底 |
| `src/mobile/MobileFileHelper_android.cpp` | Android SAF/JNI 实现；`#if defined(Q_OS_ANDROID)` 包裹 |
| `src/mobile/MobileRhiItem.h/.cpp` | QML 类型 `RhiView`（`QQuickRhiItem`）及渲染线程实现 |
| `src/qml/MobileViewer.qml` | 主界面：`StackView`、工具栏、`RhiView`、手势层、HUD、动画控制、提示横幅 |
| `src/qml/ViewerToolBar.qml` | 顶部工具栏（打开/面板/适配/投影/截图） |
| `src/qml/GestureArea.qml` | 手势覆盖层（`PinchArea` + `MouseArea`） |
| `src/qml/ModelListPage.qml` | 演示曲面 + 最近打开 + 系统文件选择（Android SAF / QML `FileDialog`） |
| `src/qml/RenderModePanel.qml` | 右侧 `Drawer`：12 渲染模式、4 显示风格、显示选项 |
| `android/AndroidManifest.xml` | Qt 模板风格清单；`QtActivity`、`glEsVersion`、Vulkan feature、`lib_name` |
| `scripts/build_android.ps1` | Windows/PowerShell 构建脚本 |
| `run.bat` | 清日志 → 启动 → 锁定 PID → 只看本进程日志的调试脚本 |
| `src/CMakeLists.txt` | Android 分支 + 桌面分支；着色器清单 |
| `CMakeLists.txt` | Qt/assimp 依赖、QML 模块、插件子目录的公共配置 |

---

## 三、工程配置与构建

### 3.1 根 `CMakeLists.txt` 要点

- `cmake_minimum_required(VERSION 3.21)`，`project(ModelViewer VERSION 1.1.0 ...)`，C++17。
- `find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets Concurrent ShaderTools GuiPrivate CorePrivate Qml Quick QuickControls2 QuickDialogs2 Network)`。Widgets 供桌面使用，Android 链接时只用其中的 Core/Gui/Quick/QuickControls2/Concurrent/ShaderTools/CorePrivate/GuiPrivate。
- Android 上 assimp 优先 `find_package(assimp CONFIG QUIET)`；找不到则 `FetchContent` 拉 `v5.4.3` 源码静态编译，并只打开 OBJ/STL/FBX/GLTF/PLY/3DS/COLLADA/MD5/BVH/X/SMD/ASE/BLEND/MS3D/MD2/MD3/AC/B3D 等 importer，禁用 exporter、tests、tools 和 `ASSIMP_BUILD_ZLIB`（用 NDK 自带 zlib）。
- `qt_standard_project_setup()`；`plugins_src/DemoPlugin` 只在 `NOT ANDROID AND NOT IOS` 时加入。

### 3.2 `src/CMakeLists.txt` 的 Android 分支

源文件用 `file(GLOB_RECURSE ...)` 收集：`core/`、`loader/`、`math/`、`renderer/`（可移植核心）和 `mobile/`（全部桥接代码）；入口显式写 `main_mobile.cpp`。目标是 `qt_add_executable(ModelViewer ...)`，在 Android 上由 Qt 打包成 `libModelViewer_<abi>.so`。

四个**必须保持**的配置点（都是实际踩坑后的结论）：

```cmake
# ① Debug 后缀必须为空：androiddeployqt 按 lib<target>_<abi>.so 找主库
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Debug 产物后缀（Android 必须为空）" FORCE)

qt_add_qml_module(ModelViewer
    URI "ModelViewer"
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"     # ② 必须显式写，否则 QML 资源落到 :/ModelViewer/
    QML_FILES qml/MobileViewer.qml ...)

set_target_properties(ModelViewer PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
    QT_ANDROID_MIN_SDK_VERSION 26
    QT_ANDROID_TARGET_SDK_VERSION 34)
target_compile_definitions(ModelViewer PRIVATE PLATFORM_ANDROID)

target_link_libraries(ModelViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2
    Qt6::Concurrent Qt6::ShaderTools Qt6::CorePrivate Qt6::GuiPrivate
    assimp::assimp)
# ③ 移动端不链接 OpenMP：NDK 的 libomp.so 不会被打进 APK
# ④ 着色器：GLSL "460,310 es" / HLSL 50 / MSL 12，PRECOMPILE + OPTIMIZE
```

说明：

1. **Debug 后缀**：缓存里若有 `CMAKE_DEBUG_POSTFIX=d`，产物变成 `libModelViewerd_arm64-v8a.so`，androiddeployqt 报 `Cannot find application binary in build dir ...`。`FORCE` 是为了压掉已有缓存值。
2. **`RESOURCE_PREFIX "/qt/qml"`**：本工程 `cmake_minimum_required` 是 3.21，QTP0001 策略停在 OLD，默认值会变成 `/`，导致 QML 落在 `:/ModelViewer/qml/...`，而入口加载的是 `qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml`，运行时报 `No such file or directory`。显式指定后 `qmldir` 也随之落在默认导入路径 `:/qt/qml` 下。
3. **OpenMP**：源码中没有任何 `#pragma omp`；若以后要用并行，必须同时把 `${OpenMP_omp_LIBRARY}` 加进 `QT_ANDROID_EXTRA_LIBS`，否则运行时 `UnsatisfiedLinkError: libomp.so not found`。
4. **着色器**：桌面和 Android 共用同一份 `MODELVIEWER_SHADER_FILES`；`*.geom` 早已不参与编译（几何着色器在 HLSL/MSL 无等价物，线段改为 CPU 生成）。`310 es` 变体给 GLES 后端；qsb 同时带 SPIR-V 变体给 Vulkan 后端。

### 3.3 包名、Activity 与产物

清单使用 Qt 6 的 Gradle namespace 风格，**没有 `package` 属性**。包名由 `QT_ANDROID_PACKAGE_NAME` 决定，当前实际为：

```
包名    org.qtproject.example.ModelViewer
Activity org.qtproject.qt.android.bindings.QtActivity
主库    libModelViewer_arm64-v8a.so
```

`android.app.lib_name` 的 meta-data 值为 `ModelViewer`（对应主库名）；SAF 不需要任何存储权限，所以清单里没有 `READ_EXTERNAL_STORAGE` / `READ_MEDIA_*`。

### 3.4 构建方式

**Qt Creator（推荐）**：选择 Android arm64-v8a kit → 构建 → 运行/部署。生成的目标在构建目录里是 `src/ModelViewer_make_apk`（也可见 `src/apk_all`），输出 APK 在 `src/android-build-ModelViewer/build/outputs/apk/debug/`。

**命令行（Qt 6.11 / Ninja 实际验证过的形式）**：

```bash
cmake --build build/Qt_6_11_1_for_Android_arm64_v8a_Debug \
      --target ModelViewer_make_apk
```

`scripts/build_android.ps1` 提供参数化的配置入口（`-QtAndroidRoot`、`-NdkRoot`、`-SdkRoot`、`-Abi`、`-BuildDir`），配置里用 `ANDROID_PLATFORM=android-26`。**注意**：脚本里写的是 `--target apk`、输出目录 `android-build/...`，与 Qt 6.11 实际生成的 `ModelViewer_make_apk` / `android-build-ModelViewer/...` 不一致，按脚本原样跑可能拿不到 APK；以 Qt Creator 或上面的命令为准。

### 3.5 调试脚本 `run.bat`

`run.bat` 位于仓库根目录，流程：检查设备 → `adb logcat -b all -c` → `am force-stop` → `am start -W -n org.qtproject.example.ModelViewer/org.qtproject.qt.android.bindings.QtActivity` → 轮询 `pidof`（最多 10 秒）→ `adb logcat --pid=<PID> -v time` 只显示本进程日志 → 结束时导出 crash 缓冲。

文件刻意保持**纯 ASCII**：cmd.exe 按系统 ANSI 代码页解析 bat，UTF-8 中文会在多字节字符中间被截断，导致后半行被当成命令执行。日志过滤器屏蔽 Adreno/libEGL/Surface/Choreographer/HWUI/MediaCodec/AAudio 等系统噪声，但**不屏蔽** `libc`、`DEBUG`、`AndroidRuntime`——原生 `Fatal signal` 与 Java 栈靠它们输出。

---

## 四、入口与 QML 类型注册

`src/main_mobile.cpp` 的关键行为（代码为准）：

```cpp
#if defined(Q_OS_ANDROID)
    const int sdkInt = QJniObject::getStaticField<jint>(
        "android/os/Build$VERSION", "SDK_INT");
    const bool useVulkan = sdkInt > 31;          // Android 12L 及以上
    qputenv("QSG_RHI_BACKEND", useVulkan ? "vulkan" : "opengl");
    if (!useVulkan) {
        QSurfaceFormat esFormat;
        esFormat.setRenderableType(QSurfaceFormat::OpenGLES);
        esFormat.setVersion(3, 1);
        esFormat.setDepthBufferSize(24);
        esFormat.setStencilBufferSize(8);
        esFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        QSurfaceFormat::setDefaultFormat(esFormat);
    }
    if (!qEnvironmentVariableIsEmpty("MYSIC_QSG_TIMING")) {
        qputenv("QSG_RENDER_TIMING", "1");
        qputenv("QSG_INFO", "1");
    }
#endif

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    auto* bridge = new MobileViewerBridge();
    auto* fileHelper = new MobileFileHelper(bridge);

    qmlRegisterSingletonInstance("ModelViewer", 1, 0, "ViewerBridge", bridge);
    qmlRegisterSingletonInstance("ModelViewer", 1, 0, "FileHelper", fileHelper);
    qmlRegisterType<MobileRhiItem>("ModelViewer", 1, 0, "RhiView");

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml"));
```

要点：

- `QSG_RHI_BACKEND` 必须在 `QGuiApplication` 构造前设置；不要写 `QT_OPENGL=es2`（Android 平台插件不读这个桌面变量）。
- 两个单例用 `qmlRegisterSingletonInstance` 直接注册已有对象，不是工厂函数；因此 `ViewerBridge` 与 `FileHelper` 是 QML 里的全局对象名，不是在 QML 中 `new` 出来的组件。
- 单例对象不设 parent，进程退出时由系统回收，避免与 engine 析构顺序竞争。
- `objectCreationFailed` 时调用 `QCoreApplication::exit(-1)`。若 QML 加载失败，紧接着可能出现 `FORTIFY ... destroyed mutex` + `SIGABRT`，那是进程在渲染线程仍活动时快速退出的连锁反应，不是根因。

---

## 五、QML UI 层

### 5.1 文件职责

| 文件 | 实际职责 |
|---|---|
| `MobileViewer.qml` | `ApplicationWindow` 根组件；`StackView` 管理主视图和模型列表页；`Material.Dark` 主题、深色背景；底部统计 HUD；加载 `BusyIndicator`；动画播放/停止按钮；提示横幅；两个 `Connections` 处理 `FileHelper` 与 `ViewerBridge` 的信号；首屏空场景自动生成演示球（`createSurface(1)`） |
| `ViewerToolBar.qml` | `ToolBar` + `RowLayout`；5 个按钮发出 `openFileRequested` / `renderPanelRequested` / `fitRequested` / `projectionToggled(bool)` / `screenshotRequested`；图标取自 `qrc:/icons/*`（`qt_add_resources` 的 `/icons` 前缀） |
| `GestureArea.qml` | `PinchArea`（外层）+ `MouseArea`（内层）组合；发出 6 个手势信号 |
| `ModelListPage.qml` | `StackView` 第二页；5 个参数曲面 + `FileHelper.recentModels()` 的最近打开；Android 走 `FileHelper.pickModel()`，其它平台走 `QtQuick.Dialogs.FileDialog`；`onAccepted` 把 `file:///` 去掉后调用 `loadModel` |
| `RenderModePanel.qml` | 右侧 `Drawer`；12 渲染模式 + 4 显示风格 + 坐标轴/网格/剔除/LOD 开关 + 法线长度滑杆；发出 `renderModeSelected` / `displayStyleSelected` |

### 5.2 `MobileViewer.qml` 中的信号连接（实际）

| QML 信号 | 调用 |
|---|---|
| `toolBar.openFileRequested` | `pageStack.push(modelListComponent)` |
| `toolBar.renderPanelRequested` | `renderPanel.open()` |
| `toolBar.fitRequested` | `ViewerBridge.fitView()` |
| `toolBar.projectionToggled(ortho)` | `ViewerBridge.setOrthographic(ortho)` |
| `toolBar.screenshotRequested` | `ViewerBridge.saveScreenshot()` |
| `renderPanel.renderModeSelected(mode)` | `ViewerBridge.setRenderMode(mode)` |
| `renderPanel.displayStyleSelected(style)` | `ViewerBridge.setDisplayStyle(style)` |
| `gestureArea.*` | 见[第六章](#六触控交互) |
| `FileHelper.fileSelected(path)` | 弹出列表页后 `ViewerBridge.loadModel(path)` |
| `ViewerBridge.loadError` / `screenshotSaved` / `screenshotFailed` / `selectionChanged` | 提示横幅 `banner.show(...)` |

### 5.3 渲染模式与显示风格枚举

与 `src/renderer/RhiRenderer.h` 完全一致：

- `RenderMode`：`0` Phong、`1` Wireframe、`2` Normals、`3` FaceNormals、`4` VertexNormals、`5` FlatShading、`6` PBR、`7` ShadowMapping、`8` Skybox、`9` Selection、`10` ClippingPlane、`11` SplitScreen。
- `DisplayStyle`：`0` Shaded、`1` Wireframe、`2` WireShaded、`3` Realistic。

### 5.4 模型列表页行为

列表分两节：`演示曲面（参数化生成）`（平面/球体/圆环/圆柱/圆锥，对应 `createSurface(0..4)`）与 `最近打开`。最近打开来自 `FileHelper.recentModels()`，条目显示 `path.split("/").pop()` 作为名字；点击“清空”调用 `FileHelper.clearRecents()` 后重建列表。

---

## 六、触控交互

### 6.1 `GestureArea.qml` 信号

`GestureArea` 使用 `PinchArea` 包住 `MouseArea`：`MouseArea` 没有 pinch 属性组，捏合由 `PinchArea` 提供；两指时 `PinchArea` 拦截触摸，单指仍交给 `MouseArea`。桌面调试时鼠标拖动/滚轮同样生效。

| 信号 | 触发 |
|---|---|
| `gestureBegin(x, y)` | `MouseArea.onPressed`，记录旋转起点 |
| `rotated(x, y, vw, vh)` | 单指/左键拖动；传绝对坐标与视口尺寸 |
| `panned(dx, dy)` | `PinchArea.onPinchUpdated` 的中心位移 |
| `zoomed(delta)` | 捏合用比例增量 `1 - scale/previousScale`；滚轮用 `angleDelta.y/120` |
| `doubleTapped()` | `MouseArea.onDoubleClicked` |
| `longPressed(x, y, vw, vh)` | `MouseArea.onPressAndHold` |

### 6.2 与桥接接口的映射

| 手势信号 | `ViewerBridge` 槽 |
|---|---|
| `gestureBegin` | `beginRotateGesture(x, y)` |
| `rotated` | `rotateAt(x, y, vw, vh)` |
| `panned` | `panGesture(dx, dy)` |
| `zoomed` | `zoomGesture(delta)` |
| `doubleTapped` | `fitView()` |
| `longPressed` | `pickObject(x, y, vw, vh)` |

实现语义与桌面 `RhiWidget` 的鼠标处理一致：`MobileViewerBridge` 内部持有 `m_lastRotatePoint` 并调用 `Camera::beginRotate`/`Camera::rotate`；`Camera` 内部自带 TrackBall 语义，因此**没有**独立的 `MobileGestureHandler` 类，也没有针对 DPI 的灵敏度系数。捏合缩放的符号在 QML 侧处理：`Camera::zoom` 正值拉远，所以放大时传负值。

其它视图接口：`resetView()`、`fitView()`、`applyViewPreset(0..8)`、`setOrthographic(bool)`、`selectMesh(int)`。

---

## 七、模型加载与文件管理

### 7.1 `MobileFileHelper`（QML 单例 `FileHelper`）

```cpp
class MobileFileHelper : public QObject {
    Q_OBJECT
public:
    explicit MobileFileHelper(QObject* parent = nullptr);
    ~MobileFileHelper() override;

public slots:
    Q_INVOKABLE void pickModel();
    Q_INVOKABLE QStringList recentModels() const;
    Q_INVOKABLE void clearRecents();

signals:
    void fileSelected(const QString& path);
    void filePickCancelled();

private:
    void saveToRecents(const QString& path);
    QStringList m_recents;   // 上限 10 条
};
```

`recentModels()` / `clearRecents()` / `saveToRecents()` 在 `MobileFileHelper.cpp` 中用 `QSettings` 持久化，键为 `mobile/recents`，最多保留 10 条。注意当前实现写入 recents 的是**拷贝到 cache 后的本地路径**（不是原始显示名），QML 侧再取 basename 显示。

### 7.2 Android SAF 流程（`MobileFileHelper_android.cpp`）

1. 构造 `ACTION_OPEN_DOCUMENT` intent，加 `OPENABLE` category，`setType("*/*")`，再用 `EXTRA_MIME_TYPES` 传多 MIME（model/obj、model/stl、model/gltf+json、model/gltf-binary、model/vnd.collada+xml、application/octet-stream、application/x-binary）。
2. `QtAndroidPrivate::startActivity(intent, 24680, callback)`，回调在 GUI 线程。需要 `Qt6::CorePrivate`（`qandroidextras_p.h`）。
3. `RESULT_OK` 且 data 有效时取 `content://` URI；用 `getLastPathSegment()` 尽力提取后缀（与 `ModelLoader::isAssimpFormat` 的格式子集对齐，不认识则不补后缀，让 Assimp 按内容探测）。
4. 通过 `ContentResolver.openInputStream` 循环拷贝到：
   `<CacheLocation>/modelview_import/<yyyyMMdd_HHmmsszzz>[.ext]`
   ——因为 Assimp/`QFile` 无法直接读 `content://`，必须落成真实路径。
5. `saveToRecents(dest)` 后 `emit fileSelected(dest)`；取消或失败 `emit filePickCancelled()`。

**条件编译细节**：该 .cpp 在**所有 include 之前**先 `#include <QtCore/qglobal.h>`，再 `#if defined(Q_OS_ANDROID)`。因为 `Q_OS_ANDROID` 是 `qglobal.h` 定义的；若把 `#if` 放在 include 之前，整个文件会被预处理掉，链接时报 `undefined symbol: MobileFileHelper::pickModel()`。对应的非 Android 兜底在 `MobileFileHelper.cpp` 里由 `#if !defined(Q_OS_ANDROID)` 提供：打一条 warning 并 `emit filePickCancelled()`，QML 侧改用 `FileDialog`。

### 7.3 异步加载（`MobileViewerBridge::loadModel`）

```
QML loadModel(path)
  → 文件存在性检查，失败 emit loadError
  → m_loading = true；emit loadingChanged / loadStarted / loadProgress(0.05)
  → QFutureWatcher<ModelAsset> + QtConcurrent::run([](path){ ModelLoader loader; return loader.loadCPU(path); })
  → finished 回调（GUI 线程）取 watcher->result()
  → m_loading = false；空 meshes → loadError
  → loadProgress(0.9) → setMeshesInternal(...) → loadProgress(1.0)
```

`setMeshesInternal()` 会把旧网格放入 `m_retired`（等渲染线程释放 GPU 后删除）、重建 `RhiMesh` 列表与场景包围盒、重置动画状态、更新 `m_modelLabel`、`m_camera.fitToView`、计算法线长度、重置裁剪面，最后 `emit meshesChanged()` + `modelLoaded(label, meshCount, triangles)` 并请求一帧。

`ModelAsset` 定义在 `src/core/Animation.h`，含 `QVector<CpuMesh> meshes` 与 `QVector<AnimationClip> animations`；`ModelLoader::loadCPU()` 返回的是 `ModelAsset`（不是早期文档里的 `QVector<CpuMesh>`）。

---

## 八、渲染、截图与统计

### 8.1 `MobileRhiItem` / `MobileRhiItemRenderer`

`MobileRhiItem : QQuickRhiItem` 暴露 `bridge` 属性；`setBridge()` 连接 `frameUpdateRequested` 信号到 `update()`，形成“Bridge 请求 → QQuickRhiItem 调度 → synchronize + render”的回路。构造时 `setSampleCount(1)`（默认关 MSAA，QML 可覆盖）。

`MobileRhiItemRenderer` 复用桌面 `RhiRenderer`（零改动），三个 `QQuickRhiItemRenderer` 重载的实际签名是：

```cpp
void initialize(QRhiCommandBuffer* cb) override;
void synchronize(QQuickRhiItem* item) override;
void render(QRhiCommandBuffer* cb) override;
```

`initialize()` 两分支：首次创建时若 `cb` 为空则置 `m_pendingInit`，推迟到 `render()`；已有 renderer 时 `setRenderTarget(renderTarget()) + rebuildPipelines()`。

### 8.2 `synchronize()` 的顺序

1. `m_bridge->reclaimRetiredMeshes()`：在渲染线程释放并删除退役网格（此时 GUI 阻塞，指针安全）。
2. 相机快照：`Camera` 禁止拷贝且 fov 没有 setter，两侧 fov 恒为默认 45°，逐字段同步 `setPosition/setTarget/setUp/setProjectionType`；aspect 在 `render()` 中按实际像素尺寸设置。
3. 值拷贝 `Light` / `Material` / `QVector<RhiMesh*>` / `m_renderPaused`，并把渲染模式、显示风格、多视图、坐标轴、网格、剔除、LOD、法线长度、裁剪面、选中项、背景色逐项设进 `RhiRenderer`。
4. `takePendingDiffuseTexture()` 取漫反射贴图；`takeUploadResetRequest()` 重置网格上传标志。
5. `takeScreenshotRequest()` 置本帧抓帧标志；上一帧抓到的 `m_grabbedImage` 经 `deliverScreenshot()` 队列投递给 GUI 线程保存。

### 8.3 后端与着色器

`main_mobile.cpp` 按 `SDK_INT > 31` 选 Vulkan，否则 GLES 3.1（`AndroidManifest.xml` 声明 `glEsVersion 0x00030001`，Vulkan feature 声明为 non-required）。两套后端共用 `qt_add_shaders(... GLSL "460,310 es" HLSL 50 MSL 12 PRECOMPILE OPTIMIZE ...)` 产出的 qsb；`*.geom` 不在编译清单内，法线/线框线段由 CPU 生成。

### 8.4 截图 readback

- 绘制完成后从离屏颜色纹理 `colorTexture()` 读回，目标格式 `QImage::Format_RGBA8888`。
- `QRhi::OpenGLES2`：走栈上 `QRhiReadbackResult`，`resourceUpdate` 时同步完成，立即组装图像。
- Vulkan 等异步后端：`new QRhiReadbackResult` + `completed` 回调只置 `m_readbackReady`，并通过 `QMetaObject::invokeMethod(item, ..., Qt::QueuedConnection)` 再请求一帧；下一帧 `render()` 开头组装图像（避免在回调里析构自身）。上一笔 readback 未回来时保留请求下一帧重试。
- 图像在随后的 `synchronize()` 交付给 Bridge，`saveScreenshotToFile()` 在 GUI 线程写文件：优先 `PicturesLocation`，失败回退 `AppLocalDataLocation/Pictures`，文件名 `modelviewer_<yyyyMMdd_HHmmss>.png`。

### 8.5 FPS 与统计

`MobileRhiItemRenderer` 用 1 秒窗口统计 FPS，凑够一秒后从 `RhiRenderer::stats()` 取对象/绘制/剔除/DC/三角/LOD 分布，经 `MobileViewerBridge::setStatsSnapshot()`（跨线程 queued 信号）拼成 `statsText`，显示在底部 HUD。文本不变时不发 `statsTextChanged`，避免 QML 无谓重绑。

---

## 九、性能与后台策略

| 策略 | 实际实现 |
|---|---|
| 动画 | `QTimer` 间隔 16ms（约 60fps），只在播放时运行；空闲不重绘 |
| 后台暂停 | 构造时连接 `QGuiApplication::applicationStateChanged` → `setRenderPaused(state != Qt::ApplicationActive)`；暂停时停动画定时器，`render()` 早退跳过 GPU 工作 |
| 大模型加载 | `QtConcurrent::run` + `QFutureWatcher`，UI 显示 `BusyIndicator` |
| 视锥剔除 / 距离 LOD | `setCullingEnabled` / `setLodEnabled` 默认 true，面板可关 |
| MSAA | `setSampleCount(1)` 默认关闭，QML 可设 2/4 |
| 帧诊断 | 设置 `MYSIC_QSG_TIMING` 环境变量后打开 `QSG_RENDER_TIMING` / `QSG_INFO` |

注意：**没有**低电量自动降帧、`QQuickWindow::setFramesPerSecond`、加载时自动减面、按平台选择 ETC2/ASTC 纹理格式等逻辑；这些只出现在早期设计稿里。

---

## 十、调试与故障排查

### 10.1 典型问题与实际修复

| 现象 | 原因 | 处理 |
|---|---|---|
| `implicit instantiation of undefined template 'QFutureWatcher<ModelAsset>'` | 只包含了 `<QtConcurrent/QtConcurrentRun>`，`qfuture.h` 只有前置声明 | 在 `MobileViewerBridge.cpp` 增加 `#include <QFutureWatcher>` |
| `reference to type 'const QPoint' could not bind to ... 'const QPoint (int, int)'` | `const QPoint current(int(x), int(y));` 被解析成函数声明（most vexing parse） | 改成 `const QPoint current{int(x), int(y)};` |
| `undefined symbol: MobileFileHelper::pickModel()` | `MobileFileHelper_android.cpp` 的 `#if defined(Q_OS_ANDROID)` 在 include 之前，宏未定义，整个文件被跳过 | 在 `#if` 前先 `#include <QtCore/qglobal.h>` |
| `Cannot find application binary in build dir .../libModelViewer_arm64-v8a.so` | Debug 缓存带了 `CMAKE_DEBUG_POSTFIX=d`，产物成 `libModelViewerd_arm64-v8a.so` | Android 分支强制 `CMAKE_DEBUG_POSTFIX ""`（FORCE） |
| `UnsatisfiedLinkError: libomp.so not found` | Android 链接了 `OpenMP::OpenMP_CXX`，但 NDK 的 libomp.so 没进 APK | Android 分支不链接 OpenMP |
| `QQmlApplicationEngine failed to load component ... No such file or directory` | QML 资源落在 `:/ModelViewer/...`，入口却加载 `:/qt/qml/ModelViewer/...` | `qt_add_qml_module` 显式 `RESOURCE_PREFIX "/qt/qml"` |
| `FORTIFY: pthread_mutex_lock called on a destroyed mutex` + `Fatal signal 6` in RenderThread | 上面的 QML 失败导致 main() 立即返回，渲染线程仍在活动 | 修复根因后消失；不要把它当第一现场 |
| bat 打印 `xxx is not recognized as an internal or external command` | UTF-8 中文被 cmd 按系统 ANSI 代码页拆断 | `run.bat` 保持纯 ASCII |

### 10.2 常用 adb 命令

```bash
adb shell pm list packages | grep ModelViewer
adb shell cmd package resolve-activity --brief org.qtproject.example.ModelViewer
adb shell am start -W -n org.qtproject.example.ModelViewer/org.qtproject.qt.android.bindings.QtActivity
adb shell pidof -s org.qtproject.example.ModelViewer
adb logcat -d -b crash -t 200
```

`run.bat` 已封装以上流程；按 Ctrl+C 结束实时日志后会再打印一次 crash 缓冲。

---

## 十一、与早期设计稿的差异 / 未实现项

下面这些出现在早期 `ModelViewer-Qt6-Android改造实施计划.md` / 本文档旧稿里，但**当前代码中不存在**，不要按它们理解或继续实现：

| 旧设计内容 | 当前实际情况 |
|---|---|
| iOS 分支、`ios/Info.plist`、`MobileFileHelper_ios.mm` | 未实现；根 CMakeLists 只有 `if(IOS) message(...)` 占位 |
| `MobileGestureHandler.h/.cpp`、DPI 灵敏度系数 | 未实现；手势直接在 QML + Bridge + Camera 内完成 |
| `QML_ELEMENT` 宏注册 `RhiView` | 未使用；`main_mobile.cpp` 用 `qmlRegisterType` 手动注册 |
| `NOGEOMSHADER` / `QT_RHI_GLES_COMPAT` 编译宏、`normal_mobile.vert/frag` | 不存在；统一走 qsb 的 `310 es` / SPIR-V 变体，线段由 CPU 生成 |
| 加载时 Assimp `SimplifyMesh`、200K 面阈值 | 不存在；`ModelLoader` 只有同步 `loadCPU()` |
| 低电量降 30fps、`onBatteryLevelChanged` | 不存在 |
| ETC2/ASTC 纹理上传、`pickTextureFormat` | 不存在 |
| `qrc:/models/*.obj` 内置模型、缩略图 | 不存在；演示内容是参数曲面 |
| `package="com.example.modelviewer"`、`READ_EXTERNAL_STORAGE` 权限、`VIEW` intent-filter | 不存在；SAF 免权限，只有 MAIN/LAUNCHER |
| `qml/` 在仓库根目录、`shaders/` 在仓库根目录 | 实际在 `src/qml/`、`src/shaders/` |
| `modelLoaded(const QString&)`、`pickCancelled()` | 实际为 `modelLoaded(label, meshCount, triangles)`、`filePickCancelled()` |
| `--target apk`（`scripts/build_android.ps1`） | Qt 6.11 生成的目标是 `ModelViewer_make_apk`；脚本该处已过时 |

此外，旧稿把 `QQuickRhiItemRenderer::initialize` 写成 `initialize(QRhi*, QRhiRenderTarget*, QRhiCommandBuffer*, const QSize&)`，实际签名是 `initialize(QRhiCommandBuffer* cb)`，需要 rhi/renderTarget 时用其访问器。

---

## 附录：实际目录结构

```
ModelViewer-Qt6/
├── CMakeLists.txt                 # 依赖、Qt 标准配置、插件子目录
├── run.bat                        # Android adb 启动 + 过滤日志（纯 ASCII）
├── android/
│   ├── AndroidManifest.xml        # QtActivity / lib_name / GLES 3.1 / Vulkan feature
│   └── res/drawable/icon.png
├── scripts/
│   └── build_android.ps1          # Windows 参数化 CMake 配置脚本
├── docs/
│   ├── ModelViewer-Qt6-移动端扩展文档.md   # 本文
│   └── ModelViewer-Qt6-Android改造实施计划.md  # 早期计划（部分已过时）
└── src/
    ├── CMakeLists.txt             # 桌面 / Android 分支、着色器清单
    ├── main.cpp                   # 桌面入口
    ├── main_mobile.cpp            # Android 入口
    ├── core/  loader/  math/  renderer/   # 与桌面共用的核心代码
    ├── ui/    net/    plugin/     # 仅桌面编译
    ├── mobile/
    │   ├── MobileViewerBridge.h/.cpp
    │   ├── MobileFileHelper.h/.cpp
    │   ├── MobileFileHelper_android.cpp
    │   └── MobileRhiItem.h/.cpp
    ├── qml/
    │   ├── MobileViewer.qml
    │   ├── ViewerToolBar.qml
    │   ├── GestureArea.qml
    │   ├── ModelListPage.qml
    │   └── RenderModePanel.qml
    └── shaders/
```
