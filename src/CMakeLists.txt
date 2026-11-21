# 源文件与着色器定义（工程级配置见仓库根目录 CMakeLists.txt）

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

# ── 着色器：统一编译为多后端 QSB（GLSL 460 / HLSL 50 / MSL 12）────────
# 说明：几何着色器在 HLSL/MSL 后端没有等价物，本项目改用 CPU 生成线段，
#       shaders/*.geom 不再参与编译。
qt_add_shaders(ModelViewer "shaders"
    GLSL "460"
    HLSL 50
    MSL 12
    PRECOMPILE
    OPTIMIZE
    FILES
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
