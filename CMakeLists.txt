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
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets Concurrent ShaderTools GuiPrivate Qml)
find_package(assimp CONFIG REQUIRED)

qt_standard_project_setup()

add_subdirectory(src)
