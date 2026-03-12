# 设置 vcpkg 工具链（必须在 project() 之前）
set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

cmake_minimum_required(VERSION 3.20)
project(ModelViewer VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准和编译器选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# 强制设置 vcpkg 工具链（必须在 project() 之前）
if(WIN32 AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain" FORCE)
endif()

# 添加 vcpkg 到 CMAKE_PREFIX_PATH
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "D:/vcpkg/installed/x64-windows")
    list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64")
endif()

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 ShaderTools)


# assimp
find_package(assimp CONFIG REQUIRED)
include_directories(${ASSIMP_INCLUDE_DIRS})
message(STATUS "ASSIMP_INCLUDE_DIRS: ${ASSIMP_INCLUDE_DIRS}")
message(STATUS "ASSIMP_LIBRARIES: ${ASSIMP_LIBRARIES}")



qt_standard_project_setup()

# 收集源文件
file(GLOB_RECURSE CORE_SOURCES "core/*.cpp" "core/*.h")
file(GLOB_RECURSE RENDERER_SOURCES "renderer/*.cpp" "renderer/*.h")
file(GLOB_RECURSE UI_SOURCES "ui/*.cpp" "ui/*.h")
file(GLOB_RECURSE LOADER_SOURCES "loader/*.cpp" "loader/*.h")
file(GLOB_RECURSE MATH_SOURCES "math/*.cpp" "math/*.h")

qt_add_executable(ModelViewer
    main.cpp
    ${CORE_SOURCES}
    ${RENDERER_SOURCES}
    ${UI_SOURCES}
    ${LOADER_SOURCES}
    ${MATH_SOURCES}
)

# ── 几何着色器：仅编译 GLSL（OpenGL 独有，无 HLSL/MSL 等价物）──────────────

qt_add_shaders(ModelViewer "shaders_geom"
    GLSL "150"
    NOHLSL          # ← 这两行是关键，缺一不可
    NOMSL
    FILES
    shaders/normals.geom
    shaders/face_normal.geom
    shaders/facenormal.geom
    shaders/vertex_normal.geom
    shaders/axis.geom
    shaders/twoside_per_fragment.geom
    shaders/subdiv.geom
)

# ── 普通着色器：多后端（GLSL / HLSL / MSL）────────────────────────────────
qt_add_shaders(ModelViewer "shaders_main"
    GLSL "100 es,120,150"
    HLSL 50
    MSL 12
    PRECOMPILE
    OPTIMIZE
    FILES
    shaders/phong.vert
    shaders/phong.frag
    shaders/wireframe.vert
    shaders/wireframe.frag
    shaders/normals.vert
    shaders/normals.frag
    shaders/background.vert
    shaders/background.frag
    shaders/face_normal.vert
    shaders/face_normal.frag
    shaders/vertex_normal.vert
    shaders/vertex_normal.frag
    shaders/axis.vert
    shaders/axis.frag
    shaders/clipped_mesh.vert
    shaders/clipped_mesh.frag
    shaders/clipping_plane.vert
    shaders/clipping_plane.frag
    shaders/debug_quad.vert
    shaders/debug_quad_depth.frag
    shaders/light_cube.vert
    shaders/light_cube.frag
    shaders/irradiance_convolution.frag
    shaders/selection.vert
    shaders/selection.frag
    shaders/shadow_mapping_depth.vert
    shaders/shadow_mapping_depth.frag
    shaders/skybox.vert
    shaders/skybox.frag
    shaders/splitScreen.vert
    shaders/splitScreen.frag
    shaders/text.vert
    shaders/text.frag
    shaders/brdf.vert
    shaders/brdf.frag
)

target_link_libraries(ModelViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::GuiPrivate Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::ShaderTools
    assimp::assimp
)
