# ModelViewer-Qt6 Android 平台改造实施计划

> 依据 `docs/ModelViewer-Qt6-移动端扩展文档.md`(下称"扩展文档")与仓库实际代码逐项核对后的落地计划。
> 本次只实现 **Android**;iOS 仅保留条件编译占位,不写实现。
> 原则:**桌面端代码路径零改动**,所有变更通过新增文件 + CMake 条件分支隔离。
> 本计划完成后直接编码;编码完成后**不编译、不运行**,由用户自行测试。

---

## 一、现状核对:扩展文档假设 vs 实际代码

实际代码与扩展文档有多处出入,逐项核对结果与决策如下:

| # | 扩展文档假设 | 实际代码 | 本次决策 |
|---|---|---|---|
| 1 | 着色器在根目录 `shaders/`,QML 在根目录 `qml/` | 着色器在 `src/shaders/`,CMake 为"根 + src"两级结构,源文件用 `GLOB_RECURSE` 收集 | QML 放 `src/qml/`(与 src 内聚),CMake 在 `src/CMakeLists.txt` 增加移动端分支 |
| 2 | `RenderMode` 9 项(Phong=0…SplitView=8) | `src/renderer/RhiRenderer.h` 定义 **12 项**:`Phong=0, Wireframe=1, Normals=2, FaceNormals=3, VertexNormals=4, FlatShading=5, PBR=6, ShadowMapping=7, Skybox=8, Selection=9, ClippingPlane=10, SplitScreen=11`;另有 `DisplayStyle` 4 项 | QML 渲染模式面板按实际 12 项对齐,并附显示风格切换 |
| 3 | `MobileGestureHandler` 直接调 `m_trackBall->rotate(dx,dy)`、`m_camera->pan(dx,dy)` | `TrackBall` 是 `Camera` 的**内部私有实现**,无注入点;实际 API:`Camera::rotate(QPoint from, QPoint to, QSize viewport)`、`pan(QPoint delta)`、`zoom(float delta)`、`fitToView(BoundingBox)`、`beginRotate(QPoint)` | **不单独建 MobileGestureHandler 类**,手势→Camera 的换算并入 `MobileViewerBridge`(几行即可,语义与桌面 `RhiWidget` 鼠标事件一致) |
| 4 | Android 需 `NOGEOMSHADER` 宏 + 新增 `normal_mobile.vert/frag` | 工程已**全面放弃几何着色器**(`src/CMakeLists.txt` 注释明确:法线线段由 CPU 生成,`*.geom` 不参与编译),现有 `normal_lines.vert/frag` 即无几何着色器路径 | **无需** `NOGEOMSHADER` 与 `normal_mobile.*`;真正的需求是给 `qt_add_shaders` 增加 **`310 es`** 变体(当前只有 `GLSL "460"`,GLES 后端无法加载 qsb) |
| 5 | CMake 单文件全量重写 | 两级 CMake(根:`find_package` Qt6 6.9 + vcpkg + `add_subdirectory(src)`、`plugins_src/DemoPlugin`;src:target + shaders + cursors 资源) | 保持两级结构,根文件仅做组件/插件条件,src 文件加 `if(ANDROID)` 分支 |
| 6 | `QQuickRhiItemRenderer::initialize(QRhi*, QRhiRenderTarget*, QRhiCommandBuffer*, QSize)` 四参签名 | Qt 6.11 实测头文件(`qquickrhiitem.h`):`initialize(QRhiCommandBuffer*)` + protected 访问器 `rhi()`、`renderTarget()`;同步点是 `synchronize(QQuickRhiItem*)`;按需渲染用 renderer 的 `update()` | 按**真实 API** 实现;`QQuickRhiItem` 提供 `sampleCount/mirrorVertically` 等属性 |
| 7 | 内置模型 `qrc:/models/teapot.obj` 等资源 | 仓库无内置模型;但有 `src/core/ParametricSurface`(`plane/sphere/torus/cylinder/cone → CpuMesh`) | ModelListPage 用参数曲面做演示模型 + 最近打开列表,不新增模型资源 |
| 8 | 新增 `res/icons/mobile/*.svg` 图标 | `res/` 已有完整 PNG 图标(`fileopen.png、fit-all.png、Ortho.png、wireframe.png…`) | 直接复用现有 PNG,打包进 qml module 资源 |
| 9 | SAF 返回 `content://` URI 直接交给 Assimp | Assimp/`QFile` 无法打开 `content://` | MobileFileHelper 先把 URI 流**拷贝到 app cache** 再加载 |
| 10 | AndroidManifest 声明 `READ_EXTERNAL_STORAGE`、`READ_MEDIA_IMAGES` | SAF `OPEN_DOCUMENT` **无需任何存储权限** | 精简权限,manifest 只声明 `glEsVersion 0x00030001` |
| 11 | 桌面/移动共用一套 `target_link_libraries`(含 Widgets、Network) | 实际无任何源文件 include Qt 私有头(`GuiPrivate` 只是链接项);`net/`、`plugin/`、`ui/` 依赖 Network/Widgets/ScriptEngine 等桌面设施 | 移动端 target 不编译 `ui/`、`net/`、`plugin/`,不链接 Widgets/Network/GuiPrivate,新增链接 `Qt6::CorePrivate`(qandroidextras_p.h) |

### 扩展文档中不可行/不采用代码的处置

| 扩展文档章节 | 问题 | 处置 |
|---|---|---|
| §7.1 Assimp 简化(`AI_CONFIG_PP_LBW_MAX_WEIGHTS`) | 该宏是**骨骼权重上限**配置,与网格简化无关,代码无效 | 不采用。移动端性能依赖渲染器已有的距离 LOD(`RhiMesh` 顶点聚类 LOD1/LOD2)+ 视锥剔除 + 默认关闭 MSAA/后处理 |
| §7.2 ETC2/ASTC 纹理压缩 | `QImage` 是未压缩 RGBA 像素,不能直接以 ETC2/ASTC 格式上传(必须预压缩数据) | 不采用。移动端保持 RGBA8 上传;diffuse 纹理本就是可选功能,模型面数才是移动端瓶颈 |
| §7.3 `win->setFramesPerSecond(fps)` | `QQuickWindow` 无此公开 API;`setUpdateBehavior` 属 `QRhiWidget` 而非 `QQuickRhiItem` | 用真实机制:`QQuickRhiItemRenderer` 本身按需渲染;仅动画播放时由 QTimer(16/33ms)驱动 `item->update()`;后台暂停由 `applicationStateChanged` 处理 |
| §2.2/§9.1 vcpkg 式 assimp 依赖 | vcpkg 不支持 Android 交叉编译 | `find_package(assimp CONFIG QUIET)` 失败时 `FetchContent` 从源码构建(静态库 + importer 白名单) |
| iOS 全部章节 | 用户本次只要 Android | 只留 `if(IOS)` 空占位,不写 `MobileFileHelper_ios.mm`、`ios/Info.plist`、`build_ios.sh` |

---

## 二、总体架构(Android)

```
┌─────────────────────────────────────────────────────┐
│  QML 层(GUI 线程)                                    │
│  MobileViewer.qml / ViewerToolBar.qml / GestureArea │
│  ModelListPage.qml / RenderModePanel.qml             │
├─────────────────────────────────────────────────────┤
│  C++ 桥接(GUI 线程)                                  │
│  MobileViewerBridge  ── Camera/Light/Material        │
│  (含手势换算、QtConcurrent 异步加载、拾取、截图)     │
│  MobileFileHelper(单例:SAF 选择 + recents/QSettings)│
│  MobileRhiItem(QQuickRhiItem 派生,QML 类型 RhiView)  │
├──────────────────────── RHI 渲染线程 ────────────────┤
│  MobileRhiItemRenderer                               │
│   ├ RhiRenderer(复用,零改动)                        │
│   ├ Camera 快照(setter 逐字段同步)                  │
│   └ mesh 列表快照 + retired 释放队列                  │
├─────────────────────────────────────────────────────┤
│  核心层(复用,零改动):                               │
│  RhiRenderer / RhiMesh / BufferPool / ModelLoader    │
│  Camera / TrackBall / BoundingBox / CpuMesh          │
│  Light / Material / Animation / ParametricSurface    │
├─────────────────────────────────────────────────────┤
│  Qt6 RHI:Android → OpenGL ES 3.1(qsb 增加 310 es)  │
└─────────────────────────────────────────────────────┘
```

### 线程模型(与桌面 `QRhiWidget` 的关键差异)

桌面 `RhiWidget::render()` 在 GUI 线程执行;`QQuickRhiItemRenderer::render()` 在 **RHI 渲染线程**执行。因此:

1. **`synchronize()` 是唯一跨线程通道**(GUI 线程与渲染线程互斥同步点)。
   Bridge 的全部状态(相机 5 字段、Light、Material、渲染参数、mesh 列表、暂停标志)在 synchronize 时拷贝到 Renderer 本地;渲染统计(Stats)反向拷回 Bridge。
2. **RhiMesh 生命周期**:GUI 线程 `new RhiMesh(nullptr, cpu)`(构造只存 CPU 数据,无 QRhi 调用,线程安全)→ 渲染线程在 `RhiRenderer::render()` 内部自动 `uploadMeshes`(已验证 RhiRenderer.cpp:1625)→ 替换模型时旧 mesh 进入 `m_retiredMeshes` 队列,由 Renderer 在下一次 `synchronize()`(渲染线程,GUI 阻塞中)调 `releaseGpu()`,随后 GUI 线程 `delete`。
3. **Camera 同步**:Camera 禁止拷贝且 fov 无 setter,但两侧 fov 一致(默认 45°,fitToView 只改 eye/target/up),故用 `setPosition/setTarget/setUp/setProjectionType/setAspectRatio` 逐字段同步,**Camera 零改动**。
4. **动画**:GUI 线程 QTimer 推进动画时间并 `setAnimationDelta`(RhiMesh 方法,重建派生数据在 GUI 线程——与桌面同款用法;顶点重烘焙涉及 CPU 数据 + 下一帧重新 upload,`m_meshesUploaded` 标志由 `resetMeshUploadFlag()` 重置)。
   注:RhiMesh::setAnimationDelta 触发的 rebuildDerivedData 是 CPU 侧,渲染线程只读 GPU 缓冲指针;变更后需 `RhiRenderer::resetMeshUploadFlag()` 且新几何由渲染线程在 render() 中重新 upload——与桌面 RhiWidget::applyAnimationTime 的既有模式一致,无新竞态。
5. **后台暂停**:`QGuiApplication::applicationStateChanged` → Bridge 置 `renderPaused` → Renderer::render() 早退,同时停动画 QTimer。

### 手势 → Camera 映射(与桌面鼠标语义对齐)

| 手势 | Bridge 方法 | Camera 调用(与 RhiWidget.cpp 同款) |
|---|---|---|
| 单指拖动 | `rotateAt(x, y, vw, vh)` | 维护 `m_lastRotatePoint`,`beginRotate(点)` 于拖动开始,随后 `rotate(from, to, QSize(vw,vh))` |
| 双指拖动 | `panGesture(dx, dy)` | `pan(QPoint(dx, dy))` |
| 双指捏合 | `zoomGesture(scaleDelta)` | `zoom(scaleDelta)`(正=放大,语义同 wheel `angleDelta/120`) |
| 双击 | `fitView()` | `fitToView(sceneBBox)` |
| 长按 | `pickObject(x, y, vw, vh)` | 纯 CPU 射线拾取(参考 `RhiWidget::pickMesh/unproject` 逻辑,用 GUI 侧 Camera 矩阵 + `RhiMesh::cpuMesh()`) |

---

## 三、文件清单

### 新增文件

| 文件 | 职责要点 |
|---|---|
| `src/main_mobile.cpp` | QGuiApplication 入口;`QSG_RHI_BACKEND=opengles`;`QQuickStyle::setStyle("Material")`;注册单例 `ViewerBridge`/`FileHelper`;加载 `qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml` |
| `src/mobile/MobileViewerBridge.h/.cpp` | QML 单例;持有 Camera/Light/Material/meshes/动画;全部 Q_INVOKABLE;异步加载(QtConcurrent);手势换算;拾取;截图请求;状态回读 |
| `src/mobile/MobileRhiItem.h/.cpp` | `MobileRhiItem : QQuickRhiItem`(`QML_ELEMENT`,类型名 `RhiView`,`Q_PROPERTY` bridge/viewportSize);内部 `MobileRhiItemRenderer`(完整渲染线程逻辑) |
| `src/mobile/MobileFileHelper.h/.cpp` | 跨平台骨架:recents(QSettings)、`pickModel()`(非 Android 平台的 QFileDialog 兜底);信号 `fileSelected` |
| `src/mobile/MobileFileHelper_android.cpp` | Android SAF:JNI 构造 `OPEN_DOCUMENT` intent(多 MIME)、`QtAndroidPrivate::startActivity` 回调、`content://` → cache 拷贝(ContentResolver InputStream 循环读) |
| `src/qml/MobileViewer.qml` | ApplicationWindow(Material 深色):RhiView + 工具栏 + 手势层 + Drawer 面板 + 底部 HUD + 模型列表页(StackView)+ BusyIndicator/错误提示 |
| `src/qml/ViewerToolBar.qml` | 打开/渲染面板/重置视图/正交切换/截图按钮,复用 `res/*.png` 图标 |
| `src/qml/GestureArea.qml` | MouseArea+pinch 组合(单指旋转/双指平移缩放/双击 fit/长按拾取,桌面鼠标同效) |
| `src/qml/ModelListPage.qml` | 最近打开(清除按钮)+ 参数曲面 5 项 + 系统文件选择器入口 |
| `src/qml/RenderModePanel.qml` | 右侧 Drawer:12 个渲染模式 + 4 个显示风格 + 开关(轴线/网格/剔除/LOD) |
| `android/AndroidManifest.xml` | QtActivity、glEsVersion 3.1、fullSensor、无存储权限 |
| `android/res/drawable/icon.png` | 应用图标(PIL 生成) |
| `scripts/build_android.ps1` | Windows 侧构建脚本(环境变量 + cmake 配置 + `--target apk`);另有 `.github`?否,单文件即可 |

### 修改文件

| 文件 | 改动 |
|---|---|
| `CMakeLists.txt`(根) | `find_package` 组件补 Quick/QuickControls2(统一查找,按平台链接);`plugins_src/DemoPlugin` 仅桌面 `add_subdirectory`;Android assimp `FetchContent` 兜底;`if(IOS)` 空占位 |
| `src/CMakeLists.txt` | ①`qt_add_shaders` GLSL 变体改为 `"460,310 es"`(桌面多带一个 es 条目无副作用);②新增 `if(ANDROID)` 移动端分支:target 源集(core/loader/math/renderer + mobile + main_mobile)、`qt_add_qml_module`、Android 属性、`CorePrivate`/Quick/QuickControls2 链接;③桌面分支原样保留(GLOB 集合不变,main.cpp 不变,cursors/POST_BUILD 保留) |

### 明确不写 / 不改

- 不写:`MobileGestureHandler.h/.cpp`(理由见差异表 #3)、`normal_mobile.vert/frag`(差异表 #4)、iOS 三件套、`MobileFileHelper_ios.mm`
- 不改:`src/ui/*`(全部)、`src/net/*`、`src/plugin/*`、`src/renderer/*`、`src/math/*`、`src/core/*`、`src/loader/*`、`src/main.cpp`、`plugins_src/*`、`src/shaders/*`(着色器源码一字不动)
- 桌面构建路径验证:用户在桌面重新配置构建时,`qt_add_shaders` 多出的 `310 es` 变体是唯一共享改动(对桌面运行时无影响,qsb 多一个条目而已)

---

## 四、CMake 改造细节

### 根 `CMakeLists.txt`

```cmake
# 组件统一查找(桌面/移动按需链接)
find_package(Qt6 6.9 REQUIRED COMPONENTS
    Core Gui Widgets Concurrent ShaderTools Qml Quick QuickControls2 Network)
find_package(assimp CONFIG QUIET)

# Android:未找到 assimp 时从源码构建(静态,importer 白名单)
if(ANDROID AND NOT assimp_FOUND)
    include(FetchContent)
    FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    ... importer 白名单(OBJ/STL/FBX/GLTF/PLY/3DS/COLLADA/MD5/BVH...)
    FetchContent_MakeAvailable(assimp)
endif()

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(plugins_src/DemoPlugin)   # 原样,包进条件
endif()
add_subdirectory(src)
# if(IOS) → message 占位,不实现
```

### `src/CMakeLists.txt` 移动端分支(关键片段)

```cmake
if(ANDROID)
    file(GLOB_RECURSE MOBILE_CORE_SOURCES "core/*.cpp" "core/*.h"
         "loader/*.cpp" "loader/*.h" "math/*.cpp" "math/*.h"
         "renderer/*.cpp" "renderer/*.h" "mobile/*.cpp" "mobile/*.h")

    qt_add_executable(ModelViewer ${MOBILE_CORE_SOURCES}
        main_mobile.cpp mobile/MobileFileHelper_android.cpp)

    qt_add_qml_module(ModelViewer URI ModelViewer VERSION 1.0
        QML_FILES qml/MobileViewer.qml ... (5 个)
        SOURCES mobile/MobileRhiItem.h mobile/MobileRhiItem.cpp   # QML_ELEMENT
        RESOURCES ../res/fileopen.png ... (工具栏用到的 PNG))

    qt_add_shaders(...)   # GLSL "460,310 es"(同一份文件列表,桌面复用)

    set_target_properties(ModelViewer PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 34)

    target_link_libraries(ModelViewer PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2
        Qt6::Concurrent Qt6::ShaderTools Qt6::CorePrivate
        assimp::assimp)
else()
    # ---------- 原桌面分支,一字不改 ----------
endif()
```

要点:
- `MobileRhiItem` 的 `QML_ELEMENT` 依赖 `qt_add_qml_module` 的 `SOURCES`(自动 moc + 注册),类型名 `RhiView`。
- 单例注册在 `main_mobile.cpp` 用 `qmlRegisterSingletonInstance`(C++ 持有实例,线程安全省心)。
- 移动端不带 `cursors` 资源、不带 POST_BUILD shader 拷贝(运行时着色器编辑是桌面功能)。
- 桌面分支的 `qt_add_shaders` 与移动端共用同一文件列表 → 抽一个变量 `SHADER_FILES` 避免两份列表漂移。

---

## 五、各组件实现要点

### 5.1 `MobileViewerBridge`(QML 单例,GUI 线程)

属性(供 QML 绑定):`loading`(bool)、`modelLabel`(QString)、`statsText`(QString)、`renderPaused`。

Q_INVOKABLE(与扩展文档 §8.3 对齐 + 实际化):
- `loadModel(path)` / `createSurface(type)`(0-4:plane/sphere/torus/cylinder/cone)
- `setRenderMode(int)` / `setDisplayStyle(int)` / `setOrthographic(bool)` / `setMultiView(bool)`
- `resetView()` / `fitView()` / `applyViewPreset(int)`(复用 `Camera::setViewPreset`)
- `rotateAt(x, y, vw, vh)` / `panGesture(dx, dy)` / `zoomGesture(scale)` / `pickObject(x, y, vw, vh)`
- `setAxisVisible(bool)` / `setGridVisible(bool)` / `setCullingEnabled(bool)` / `setLodEnabled(bool)`
- `setNormalLength(float)` / `setClipPlane(x,y,z,w)` / `setBackgroundColors(...)`(轻量)
- `playAnimation()` / `pauseAnimation()` / `stopAnimation()` / `setAnimationSpeed(float)`
- `saveScreenshot()`(存 `QStandardPaths::PicturesLocation` 下 app 目录)
- `selectMesh(int)` / `meshList()`(返回 name 列表供 QML 对象选择)

信号:`loadStarted` / `loadProgress(real)` / `modelLoaded(name,count,tris)` / `loadError(msg)` / `renderModeChanged(int)` / `selectionChanged(int)` / `screenshotSaved(path)` / `statsChanged()`。

加载流程:`loadStarted` → `QtConcurrent::run(loadCPU)` → 完成(GUI 线程回调)→ `new RhiMesh(nullptr, cpu)` ×N → 旧 mesh 入 retired 队列 → `camera.fitToView(bbox)` → `modelLoaded`。动画剪辑存 Bridge,`QTimer(16ms)` 播放推进(调 `RhiMesh::setAnimationDelta` + `RhiRenderer::resetMeshUploadFlag`,通过 dirty 标志在 synchronize 带给渲染线程)。

### 5.2 `MobileRhiItem` / `MobileRhiItemRenderer`

- Item:`Q_PROPERTY(MobileViewerBridge* bridge)`、`Q_PROPERTY` viewport 尺寸回报;`createRenderer()` 返回 Renderer;默认 `sampleCount=1`(移动端省电)、`mirrorVertically` 按截图需要。
- Renderer:
  - `initialize(cb)`:首次创建 `RhiRenderer(rhi(), renderTarget())` 并按 Bridge 状态初始化;rt/rhi 变化时 `setRenderTarget + rebuildPipelines`(对应桌面 `RhiWidget::initialize` 的两分支)。
  - `synchronize(item)`:从 Bridge 拷:相机 5 字段、Light/Material、mesh 指针列表、RenderMode/DisplayStyle/MultiView/Axis/Grid/Cull/LOD/NormalLength/ClipPlane/背景色、暂停标志、`resetUploadFlag` 请求;释放 retired mesh GPU 资源;把 `m_renderer->stats()` + fps 拷回 Bridge。
  - `render(cb)`:paused 早退;`pixelSize → aspect` 设置相机快照;`m_renderer->render(cb, meshes, cam, light, mat)`(内部自动 uploadMeshes,与桌面 `RhiWidget::render` 完全一致);截图请求时在同一帧末尾对 `colorTexture()` 做 `readBackTexture` → QImage → mirror → 交付 Bridge。
  - 析构(渲染线程):`delete m_renderer`。
- 帧驱动:交互/参数变化 → Bridge 置 dirty → `item->update()`;动画播放时 QTimer 驱动;空闲不重绘。

### 5.3 `MobileFileHelper`(单例)

- `pickModel()`:Android 走 `MobileFileHelper_android.cpp`(SAF);其余平台 QFileDialog 兜底便于桌面调试 QML。
- recents:QSettings(`modelviewer/recents` 字符串列表,上限 10),`recentModels()` / `clearRecents()` Q_INVOKABLE。
- Android 实现要点:
  - `QJniObject("android/content/Intent","(Ljava/lang/String;)V", ACTION_OPEN_DOCUMENT)` + `addCategory(OPENABLE)` + `setType("*/*")` + `putExtra(EXTRA_MIME_TYPES, String[])`(QJniEnvironment 构造 jobjectArray)。
  - MIME:obj/stl/ply/3ds/dae/glb/fbx/…按 `ModelLoader::isAssimpFormat` 常用集合 + `application/octet-stream`。
  - `QtAndroidPrivate::startActivity(intent, 42, 回调)`:RESULT_OK 时取 `data.getData()` → `contentResolver.openInputStream(uri)` → jbyte 缓冲循环读 → 写 `QStandardPaths::writableLocation(CacheLocation)/modelview_import/<时间戳>_<原名>` → `emit fileSelected(本地路径)`。
  - 失败/取消 → `filePickCancelled`(QML 关 BusyIndicator)。

### 5.4 QML

- `GestureArea.qml`:最终实现采用 **MouseArea + pinch 组合**(全平台稳定,触摸合成鼠标 + 双指捏合,语义与桌面 RhiWidget 鼠标事件一一对应),而非扩展文档的 DragHandler/PinchHandler。信号:`gestureBegin(x,y)`(对应 Camera::beginRotate)、`rotated(x,y,vw,vh)`(绝对坐标+视口)、`panned(dx,dy)`、`zoomed(delta)`(正=放大)、`doubleTapped()`、`longPressed(x,y,vw,vh)`;桌面调试时鼠标拖动/滚轮同样生效。
- `RenderModePanel.qml`:12 项模式(枚举值硬编码对齐 RhiRenderer)+ 4 项显示风格 + 4 个 Switch(轴线/网格/剔除/LOD)。
- `MobileViewer.qml`:StackView(主视图 ↔ 模型列表);底部 HUD(对象数/三角/drawCalls/FPS);Material Dark;`Connections target: FileHelper` 接 `onFileSelected → bridge.loadModel(path)`;错误用 Material Snackbar 风格自绘横幅。
- 工具栏图标映射:`fileopen.png`(打开)、`section.png`(渲染面板)→ 用 `wireframe.png` 更达意、`fit-all.png`(fit)、`Ortho.png/Perspective.png`(投影)、`export.png`(截图)。均为 qrc 资源,图标文件本身已在 res/。

### 5.5 Android 原生

- Manifest:Qt 6.9+ 模板结构(QtActivity、`android:exported="true"`、`screenOrientation="fullSensor"`、`configChanges` 常规集、`uses-feature glEsVersion 0x00030001 required`)。**不请求任何存储权限**(SAF 免权限)。
- `android/res/drawable/icon.png`:脚本用 Pillow 生成(浅色立方体线框风格 192×192)。
- `scripts/build_android.ps1`:参数化 `QT_ANDROID_ROOT / ANDROID_NDK_ROOT / ANDROID_SDK_ROOT / ANDROID_ABI(默认 arm64-v8a)`,执行 configure + `cmake --build --target apk`,输出 APK 路径。

---

## 六、实施顺序

1. **CMake**(根 + src):移动端分支先成型,文件暂缺时逐个补。
2. **入口与桥接**:`main_mobile.cpp` → `MobileViewerBridge` → `MobileRhiItem`(含 Renderer)。
3. **文件访问**:`MobileFileHelper` + Android JNI。
4. **QML** 5 个文件。
5. **Android 原生**:manifest + 图标 + 构建脚本。
6. **静态自检**(不编译):grep 确认桌面源码零改动;API 签名与 Qt 6.11 头文件比对;着色器列表与 src/CMakeLists 一致;QML import/类型名一致性;JNI 方法签名核对。

---

## 七、风险与用户自测清单

### 环境前提(用户侧)

- Qt 6.9+ for Android(`arm64-v8a`;本地现有 6.11.1 桌面 kit 仅用于 API 核对,Android kit 需在 Maintenance Tool 安装)
- Android SDK 34+ / NDK r26+ / JDK 17;`scripts/build_android.ps1` 或 Qt Creator 均可
- assimp:优先 `find_package`;无则 FetchContent 需要网络(github)

### 已知风险点(按概率排序)

1. **着色器 GLES 降级**:`qt_add_shaders` 的 `310 es` 变体由 SPIR-V 交叉编译生成。已抽查 `phong/pbr/post` 均为标准 float/std140/sampler2D 写法,无 ES 不可降级特性(无 double、无几何着色器);若个别文件构建报错,失败信息会精确到文件,替换/修补对应源即可(构建期错误,不影响桌面)。
2. **QQuickRhiItem 线程细节**:synchronize/render 的调用时序以 Qt 官方 rhiitem 示例为准(本计划已按 6.11 头文件核对);若真机上出现纹理未就绪,优先检查 initialize 的 rt 重建分支。
3. **assimp FetchContent**:NDK 交叉编译 assimp 官方支持,但 tag 与 NDK 版本组合偶有告警;可换系统源或降级 v5.3.1。
4. **JNI 细节**:`startActivity` 回调签名、`openInputStream` 循环读已按 Qt 6.9 文档核对;若个别 ROM 行为差异,失败路径都有 `filePickCancelled`/`loadError` 信号兜底,可观察日志。

### 首测验证点(建议顺序)

1. APK 启动 → 空场景背景渐变渲染(验证 RHI/QSB/es 变体)
2. 模型列表页 → 生成球/环面 → 显示(验证 Bridge→Renderer 全链路)
3. 单指旋转 / 双指平移缩放 / 双击 fit(手势)
4. 渲染面板切换 12 模式与显示风格
5. SAF 打开一个 .obj / .glb(验证拷贝+异步加载+进度)
6. 长按拾取高亮;播放动画;截图后检查 Pictures 目录
7. Home 键后台再回前台(暂停/恢复);旋转屏幕(orientation 变化)

---

## 八、桌面端零改动承诺

- `git status` 应只出现:`CMakeLists.txt`、`src/CMakeLists.txt` 两个修改 + `src/mobile/`、`src/qml/`、`src/main_mobile.cpp`、`android/`、`scripts/`、本计划文档等新增。
- 桌面重新构建唯一共享变化:qsb 文件多一个 `310 es` 条目(对桌面运行时零影响,QRhi 按后端自选条目)。

---

## 九、实施落地记录(编码完成,未编译)

以下文件已全部落地(与本计划第三节清单一一对应,桌面源码零改动,已用 git 核对):

- 修改:`CMakeLists.txt`(组件/assimp 兜底/插件条件/iOS 占位)、`src/CMakeLists.txt`(着色器共享清单 + 310 es 变体 + `if(ANDROID)` 移动端分支 + `return()` 提前返回,桌面分支保持原样)
- 新增:`src/main_mobile.cpp`、`src/mobile/MobileViewerBridge.h/.cpp`、`src/mobile/MobileRhiItem.h/.cpp`、`src/mobile/MobileFileHelper.h/.cpp`、`src/mobile/MobileFileHelper_android.cpp`、`src/qml/` 5 个 QML、`android/AndroidManifest.xml`、`android/res/drawable/icon.png`、`scripts/build_android.ps1`

编码期间的额外核对与关键决策(静态自检,未编译):

1. **Qt API 已对照本地 Qt 6.11 头文件/源码**:QQuickRhiItemRenderer 虚函数为 `initialize(QRhiCommandBuffer*)/synchronize(QQuickRhiItem*)/render(QRhiCommandBuffer*)` + protected `rhi()/renderTarget()/colorTexture()`(扩展文档的四参签名不存在);`QtAndroidPrivate::startActivity(intent, code, std::function<void(int,int,const QJniObject&)>)` 签名与 qtbase 6.11 源码一致;Android 上下文改用公开 API `QNativeInterface::QAndroidApplication::context()`(qandroidextras_p.h 里没有 activity())。
2. **RhiMesh 上传链路已验证**:`RhiRenderer::uploadMeshes` 内部会先 `mesh->setRhi(m_rhi)` 再 `upload(batch)`,因此 GUI 线程 `new RhiMesh(nullptr, cpu)` → 渲染线程自动补 rhi 的桌面模式在移动端同样成立。
3. **pass 管理已验证**:`RhiRenderer::render()` 自带 beginPass/endPass(含阴影/后处理子 pass),与 QRhiWidget / QQuickRhiItem "子类自管 pass"的约定一致,直接调用即可。
4. **线程安全设计落地**:synchronize 同步点完成 ① retired 网格 releaseGpu+delete ② 相机逐字段快照(fov 两侧恒 45°,无需改 Camera)③ 参数同步 ④ take* 一次性标志 ⑤ 截图交付与统计 queued 回写;render() 只读快照。
5. **截图**:GLES 后端 readback 同步完成,代码中已标注 Vulkan/Metal 后端需改 completed 回调、以及若真机上图像颠倒补 `.mirrored()` 的位置。
6. 手势层最终采用 MouseArea+pinch 组合(见 §5.4 已同步更新),QML 的非 Android 平台用 QtQuick.Dialogs FileDialog 兜底。

构建:环境就绪后运行 `scripts/build_android.ps1`(或 Qt Creator 打开根 CMakeLists 选 Android kit)。
