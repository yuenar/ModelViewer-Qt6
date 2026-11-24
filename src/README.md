# ModelViewer-Qt6 源代码目录

本目录包含桌面端与 Android 移动端的全部源代码。两套入口共用 `core/`、`loader/`、`math/`、`renderer/`；`ui/`、`net/`、`plugin/` 只属于桌面端。

## 目录结构

```
src/
├── main.cpp              # 桌面入口（QApplication + MainWindow）
├── main_mobile.cpp       # Android 入口（QGuiApplication + Qt Quick）
├── CMakeLists.txt        # 桌面 / Android 两套构建分支、着色器清单
├── core/                 # 核心数据结构（CpuMesh / Light / Material / Animation / ParametricSurface）
├── loader/               # 模型加载器（ModelLoader → ModelAsset）
├── math/                 # 数学库（BoundingBox / Camera / TrackBall / Frustum）
├── renderer/             # Qt RHI 渲染核心（RhiRenderer / RhiMesh / BufferPool / ShaderCompiler）
├── ui/                   # 桌面 UI（Qt Widgets，移动端不编译）
├── net/  plugin/         # 桌面协作会话与脚本插件（移动端不编译）
├── mobile/               # Android 桥接层
│   ├── MobileViewerBridge.h/.cpp       # QML 单例 ViewerBridge：场景状态、手势、加载、拾取、动画、截图
│   ├── MobileFileHelper.h/.cpp         # QML 单例 FileHelper：recents + 非 Android 兜底
│   ├── MobileFileHelper_android.cpp    # Android SAF / JNI，content:// → cache
│   └── MobileRhiItem.h/.cpp            # QML 类型 RhiView（QQuickRhiItem）+ 渲染线程实现
├── qml/                  # Android Qt Quick 界面
│   ├── MobileViewer.qml                # 主界面（StackView / HUD / 动画 / 反馈横幅）
│   ├── ViewerToolBar.qml               # 顶部工具栏
│   ├── GestureArea.qml                 # 手势覆盖层（PinchArea + MouseArea）
│   ├── ModelListPage.qml               # 演示曲面 + 最近打开 + 文件选择
│   └── RenderModePanel.qml             # 右侧 Drawer：渲染模式 / 显示风格 / 显示选项
└── shaders/              # 着色器源码（GLSL 460 + 310 es / HLSL 50 / MSL 12 变体）
```

## 模块说明

### core/ - 核心数据结构
- `CpuMesh.h` - CPU 端网格（顶点/索引/包围盒/名称/漫反射贴图/节点路径/骨骼蒙皮数据）
- `Light.h` / `Material.h` - 光照与材质参数
- `Animation.h` - `AnimationClip` / `AnimationChannel` / `ModelAsset`（移动端异步加载的返回类型）
- `ParametricSurface.h/.cpp` - 5 种参数曲面（平面、球、环、圆柱、圆锥），移动端模型列表页使用

### loader/ - 模型加载器
- `ModelLoader.cpp/h` - 基于 Assimp 的同步加载接口：`ModelAsset loadCPU(const QString& filePath)`；`ModelAsset` 同时携带 `meshes` 和 `animations`
- Android 上 Assimp 找不到时由 CMake FetchContent 静态编译（v5.4.3，importer 白名单）

### math/ - 数学库
- `BoundingBox.cpp/h` - 包围盒（`expand()`、`radius()`、`isEmpty()`）
- `Camera.cpp/h` - 相机控制（投影切换、视图预设、旋转/平移/缩放、适配视口），内部自带 TrackBall 语义
- `TrackBall.cpp/h` - 轨迹球控制器（桌面 RhiWidget 使用；移动端不单独实例化）
- `Frustum.h` - 视锥剔除

### renderer/ - 渲染器
- `RhiRenderer.cpp/h` - 主渲染器（12 种 `RenderMode`、4 种 `DisplayStyle`、统计 `RenderStats`）
- `RhiMesh.cpp/h` - GPU 端网格数据与上传标志管理
- `BufferPool.cpp/h` - GPU 缓冲池
- `ShaderCompiler.cpp/h` - 运行时 qsb 编译（桌面 Shader Editor 用）

### ui/ - 桌面 UI（移动端不编译）
- `MainWindow.cpp/h`、`RhiWidget.cpp/h`、`LightDialog.cpp/h`、`MaterialDialog.cpp/h` 等

### mobile/ - Android 桥接层
- `MobileViewerBridge` 只在 GUI 线程访问；带 `[SG-sync]` 标记的只读/一次性接口供渲染线程在 `synchronize()` 内调用
- `MobileRhiItemRenderer` 复用 `RhiRenderer`，渲染线程只使用 `synchronize()` 拷贝的快照
- `MobileFileHelper_android.cpp` 用 `Q_OS_ANDROID` 条件编译；`#if` 之前必须先包含 `<QtCore/qglobal.h>`

### qml/ - Android Qt Quick 界面
- 主 QML 资源路径：`qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml`（要求 `qt_add_qml_module(... RESOURCE_PREFIX "/qt/qml")`）
- 桥接对象注册名：`ViewerBridge`、`FileHelper`；3D 控件注册名：`RhiView`

### shaders/ - 着色器
GLSL/HLSL/MSL 着色器源码，`qt_add_shaders` 使用 `GLSL "460,310 es" / HLSL 50 / MSL 12`，PRECOMPILE + OPTIMIZE。`*.geom` 不参与编译，线段由 CPU 生成。

## 技术特点

- **双入口**：桌面 Qt Widgets / Android Qt Quick，共用同一套渲染核心
- **跨平台渲染**：Qt RHI；桌面可用 OpenGL/D3D/Vulkan/Metal，Android 按 `SDK_INT > 31` 选 Vulkan，否则 OpenGL ES 3.1
- **现代 C++**：C++17
- **异步加载**：`QtConcurrent::run` + `QFutureWatcher<ModelAsset>`；Android 文件先由 SAF 拷贝到 cache
- **移动端按需渲染**：动画定时器仅播放时运行，后台自动暂停渲染

## 构建要求

- Qt 6.9+（当前验证版本 6.11.1 for Android arm64-v8a）
- CMake 3.21+；Android 另需 NDK、SDK（platform 34）、JDK 17
- Assimp 5.x（桌面用 vcpkg/系统包；Android 可 FetchContent）
- 移动端必须保持：`CMAKE_DEBUG_POSTFIX ""`、`RESOURCE_PREFIX "/qt/qml"`、不链接 OpenMP（详见 `docs/ModelViewer-Qt6-移动端扩展文档.md` 第三章）

## 入口点

- 桌面：`main.cpp` 创建 `QApplication` 与 `MainWindow`
- Android：`main_mobile.cpp` 创建 `QGuiApplication`，注册 QML 单例与类型，加载 `qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml`
