# 示例 C++ ABI 插件（构建到 <build>/<config>/plugins）
qt_add_library(DemoPlugin MODULE
    DemoPlugin.cpp
    DemoPlugin.h
)

target_link_libraries(DemoPlugin PRIVATE Qt6::Core)
target_include_directories(DemoPlugin PRIVATE
    "${CMAKE_SOURCE_DIR}/src/plugin"
)

set_target_properties(DemoPlugin PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${_cfg}" _cfg_upper)
    set_target_properties(DemoPlugin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg_upper} "${CMAKE_BINARY_DIR}/${_cfg}/plugins"
        LIBRARY_OUTPUT_DIRECTORY_${_cfg_upper} "${CMAKE_BINARY_DIR}/${_cfg}/plugins"
    )
endforeach()
