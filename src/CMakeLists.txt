# 源文件与着色器定义（工程级配置见仓库根目录 CMakeLists.txt）

# ── 着色器文件清单（桌面 / Android 移动端共享同一份列表）──────────
# 说明：GLSL 变体统一为 "460,310 es"——桌面继续使用 460 条目，
#       Android OpenGL ES 3.1 后端使用 310 es 条目（qsb 多带一个
#       变体对桌面运行时无影响；着色器源码本身不改动）。
set(MODELVIEWER_SHADER_FILES
    shaders/phong.vert
    shaders/phong.frag
    shaders/flat.vert
    shaders/flat.frag
    shaders/wireframe.vert
    shaders/wireframe.frag
    shaders/wire_overlay.frag
    shaders/normals.vert
    shaders/normals.frag
    shaders/normal_lines.vert
    shaders/normal_lines.frag
    shaders/background.vert
    shaders/background.frag
    shaders/pbr.vert
    shaders/pbr.frag
    shaders/shadow_depth.vert
    shaders/shadow_depth.frag
    shaders/shadowed_phong.vert
    shaders/shadowed_phong.frag
    shaders/skybox.vert
    shaders/skybox.frag
    shaders/selection.vert
    shaders/selection.frag
    shaders/clipped_mesh.vert
    shaders/clipped_mesh.frag
    shaders/cap.vert
    shaders/cap.frag
    shaders/text3d.vert
    shaders/text3d.frag
    shaders/splitScreen.vert
    shaders/splitScreen.frag
    shaders/post.vert
    shaders/post.frag
    shaders/axis.vert
    shaders/axis.frag
)

# ══════════════════════════════════════════════════════════════════
#  Android 移动端（Qt Quick + QQuickRhiItem + GLES 3.1）
#  - 不编译 ui/ net/ plugin/（桌面专属：MainWindow、协作会话、脚本插件）
#  - 入口 main_mobile.cpp，QML 位于 src/qml/
#  - 图标复用 res/ 下的现有 PNG
# ══════════════════════════════════════════════════════════════════
if(ANDROID)
    file(GLOB_RECURSE MOBILE_PORTABLE_SOURCES
        "core/*.cpp" "core/*.h"
        "loader/*.cpp" "loader/*.h"
        "math/*.cpp" "math/*.h"
        "renderer/*.cpp" "renderer/*.h")
    file(GLOB_RECURSE MOBILE_BRIDGE_SOURCES
        "mobile/*.cpp" "mobile/*.h")

    # Android 部署要求产物文件名严格为 lib<target>_<abi>.so：
    # androiddeployqt 用 application-binary（= 目标名）拼出该文件名，去
    # android-build-*/libs/<abi>/ 下查找。如果 Debug 构建带后缀 d，
    # 实际产物是 libModelViewerd_arm64-v8a.so，就会报
    # "Cannot find application binary in build dir ..."。
    # 这里强制置空（覆盖缓存里的 d），必须在 qt_add_executable 之前。
    set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Debug 产物后缀（Android 必须为空）" FORCE)

    qt_add_executable(ModelViewer
        main_mobile.cpp
        ${MOBILE_PORTABLE_SOURCES}
        ${MOBILE_BRIDGE_SOURCES}
    )

    # QML 模块（C++ 类型 RhiView / 单例 ViewerBridge / FileHelper
    # 在 main_mobile.cpp 中手动注册到同一 URI）
    #
    # RESOURCE_PREFIX 必须显式写成 /qt/qml：
    # 本工程 cmake_minimum_required 是 3.21，QTP0001 策略停留在 OLD，
    # qt_add_qml_module 默认把资源前缀取成 "/"，于是 MobileViewer.qml 落在
    #   :/ModelViewer/qml/MobileViewer.qml
    # 而 main_mobile.cpp 里加载的是 Qt 6 标准布局
    #   qrc:/qt/qml/ModelViewer/qml/MobileViewer.qml
    # → 运行时报 "QQmlApplicationEngine failed to load component ...
    #   No such file or directory"。写成 /qt/qml 后两者一致，同时
    # qmldir 也落在默认导入路径 :/qt/qml 下，"import ModelViewer" 可正常解析。
    qt_add_qml_module(ModelViewer
        URI "ModelViewer"
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"
        QML_FILES
            qml/MobileViewer.qml
            qml/ViewerToolBar.qml
            qml/GestureArea.qml
            qml/ModelListPage.qml
            qml/RenderModePanel.qml
    )

    # 移动端工具栏图标（复用桌面 res/ 资源，qrc:/icons/...）
    qt_add_resources(ModelViewer "mobile_icons"
        PREFIX "/icons"
        BASE "../res"
        FILES
            ../res/fileopen.png
            ../res/wireframe.png
            ../res/fit-all.png
            ../res/Ortho.png
            ../res/Perspective.png
            ../res/export.png
            ../res/shaded.png
            ../res/section.png
    )

    qt_add_shaders(ModelViewer "shaders"
        GLSL "460,310 es"
        HLSL 50
        MSL 12
        PRECOMPILE
        OPTIMIZE
        FILES ${MODELVIEWER_SHADER_FILES}
    )

    set_target_properties(ModelViewer PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
    target_compile_definitions(ModelViewer PRIVATE PLATFORM_ANDROID)

    # Qt6::CorePrivate：qandroidextras_p.h（SAF / JNI 调用需要）
    # Qt6::GuiPrivate：rhi/qrhi.h 等 QtGui 私有头（RhiRenderer/BufferPool/
    #                 RhiMesh 需要，与桌面构建一致）
    target_link_libraries(ModelViewer PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2
        Qt6::Concurrent Qt6::ShaderTools Qt6::CorePrivate Qt6::GuiPrivate
        assimp::assimp
    )

    # 移动端不再链接 OpenMP：源码里没有任何 #pragma omp（全仓库 grep 为空），
    # 而 NDK clang 的 libomp.so 位于 toolchains/llvm/prebuilt/*/lib/clang/<ver>
    # 目录，androiddeployqt 不会把它打进 APK，导致运行时：
    #   UnsatisfiedLinkError: dlopen failed: library "libomp.so" not found
    #   needed by libModelViewer_arm64-v8a.so
    # 若以后真的要并行化，必须同时把 libomp.so 加入打包：
    #   set_target_properties(ModelViewer PROPERTIES
    #       QT_ANDROID_EXTRA_LIBS "${OpenMP_omp_LIBRARY}")

    # 移动端不需要：桌面光标资源、POST_BUILD 着色器拷贝（运行时
    # 着色器编辑是桌面 ShaderEditor 的功能）
    return()
endif()

# ══════════════════════════════════════════════════════════════════
#  桌面端（Windows / macOS / Linux，原有构建，保持不变）
# ══════════════════════════════════════════════════════════════════

# 收集源文件
file(GLOB_RECURSE CORE_SOURCES "core/*.cpp" "core/*.h")
file(GLOB_RECURSE RENDERER_SOURCES "renderer/*.cpp" "renderer/*.h")
file(GLOB_RECURSE UI_SOURCES "ui/*.cpp" "ui/*.h")
file(GLOB_RECURSE LOADER_SOURCES "loader/*.cpp" "loader/*.h")
file(GLOB_RECURSE MATH_SOURCES "math/*.cpp" "math/*.h")
file(GLOB_RECURSE NET_SOURCES "net/*.cpp" "net/*.h")
file(GLOB_RECURSE PLUGIN_SOURCES "plugin/*.cpp" "plugin/*.h")

qt_add_executable(ModelViewer
    main.cpp
    ${CORE_SOURCES}
    ${RENDERER_SOURCES}
    ${UI_SOURCES}
    ${LOADER_SOURCES}
    ${MATH_SOURCES}
    ${NET_SOURCES}
    ${PLUGIN_SOURCES}
)

# ── 着色器：统一编译为多后端 QSB（GLSL 460+310es / HLSL 50 / MSL 12）
# 说明：几何着色器在 HLSL/MSL 后端没有等价物，本项目改用 CPU 生成线段，
#       shaders/*.geom 不再参与编译。
#       310 es 变体供 Android OpenGL ES 3.1 后端使用（对桌面无影响）。
qt_add_shaders(ModelViewer "shaders"
    GLSL "460,310 es"
    HLSL 50
    MSL 12
    PRECOMPILE
    OPTIMIZE
    FILES ${MODELVIEWER_SHADER_FILES}
)

target_link_libraries(ModelViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::ShaderTools Qt6::GuiPrivate
    Qt6::Qml
    Qt6::Network
    assimp::assimp
)

# ── 运行时着色器编辑器：把 GLSL 源码复制到可执行文件旁 ─────────────
add_custom_command(TARGET ModelViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:ModelViewer>/shaders"
    COMMENT "Copy shader sources for runtime editing"
)

# ── 光标资源（旋转/平移/缩放）──────────────────────────────────────
qt_add_resources(ModelViewer "cursors"
    PREFIX "/res"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../res"
    FILES
        ../res/rotatecursor.png
        ../res/pancursor.png
        ../res/zoomcursor.png
        ../res/window-zoom-cursor.png
)

# 如果找到 OpenMP，链接它
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(ModelViewer PRIVATE OpenMP::OpenMP_CXX)
endif()
