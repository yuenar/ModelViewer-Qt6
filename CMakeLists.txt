cmake_minimum_required(VERSION 3.21)

# ── vcpkg 工具链（Windows，存在才启用）──────────────────────────────
if(WIN32 AND EXISTS "D:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    # 关闭 applocal 自动拷贝（运行时通过 PATH 提供依赖，避免某些环境的
    # PowerShell/AMSI 导致后置步骤崩溃）
    set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "Copy vcpkg DLLs next to the app" FORCE)
    set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain" FORCE)
endif()

project(ModelViewer VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt 安装位置自动探测（可用 -DQT_ROOT=... 覆盖）──────────────────
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "D:/vcpkg/installed/x64-windows")
    if(NOT DEFINED QT_ROOT)
        file(GLOB _qt_kits "D:/Qt/6.*/msvc2022_64")
        if(_qt_kits)
            list(SORT _qt_kits)
            list(GET _qt_kits -1 QT_ROOT)
        endif()
    endif()
    if(DEFINED QT_ROOT AND EXISTS "${QT_ROOT}")
        message(STATUS "Using Qt from: ${QT_ROOT}")
        list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
    endif()
elseif(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# ── 依赖 ────────────────────────────────────────────────────────────
# 组件统一查找，桌面 / Android 各自按需链接：
#   桌面  ：Widgets + GuiPrivate + Network（MainWindow / 私有头 / 协作会话）
#   Android：Quick + QuickControls2 + CorePrivate（QML / Material / qandroidextras_p.h）
find_package(Qt6 6.9 REQUIRED COMPONENTS
    Core Gui Widgets Concurrent ShaderTools GuiPrivate CorePrivate Qml Quick
    QuickControls2 QuickDialogs2 Network)

# ── assimp：桌面使用 vcpkg 等系统安装；Android 交叉编译时找不到则
#    从源码 FetchContent（静态库 + importer 白名单，与桌面常用格式对齐）
if(ANDROID)
    find_package(assimp CONFIG QUIET)
    if(NOT assimp_FOUND)
        message(STATUS "assimp not found for Android, fetching from source")
        include(FetchContent)
        set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
        set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ZLIB OFF CACHE BOOL "" FORCE)   # 使用 NDK 自带 zlib
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)   # 静态链接进 APK
        set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
        set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
        # 与 ModelLoader::isAssimpFormat 的常用格式子集对齐
        set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_PLY_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_MD5_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_BVH_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_X_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_SMD_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ASE_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_MS3D_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_MD2_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_MD3_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_AC_IMPORTER ON CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_B3D_IMPORTER ON CACHE BOOL "" FORCE)
        FetchContent_Declare(assimp
            GIT_REPOSITORY https://github.com/assimp/assimp.git
            GIT_TAG v5.4.3)
        FetchContent_MakeAvailable(assimp)
    endif()
else()
    find_package(assimp CONFIG REQUIRED)
endif()

qt_standard_project_setup()

# ── 子目录：桌面插件（QPluginLoader 脚本插件体系，移动端不编译）─────
if(NOT ANDROID AND NOT IOS)
    add_subdirectory(plugins_src/DemoPlugin)
endif()

add_subdirectory(src)

# ── iOS：本轮不实现（仅保留占位，条件编译入口在 src/CMakeLists.txt）──
if(IOS)
    message(STATUS "iOS build is not implemented in this iteration (Android only)")
endif()
