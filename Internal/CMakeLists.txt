cmake_minimum_required(VERSION 3.16)
project(cs2_internal)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "x86 not supported — CS2 is 64-bit only")
endif()

add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT $<$<CONFIG:Release,RelWithDebInfo>:ProgramDatabase>)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../minhook-master ${CMAKE_CURRENT_BINARY_DIR}/minhook)

set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/backends/imgui_impl_dx11.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/backends/imgui_impl_win32.cpp
)

add_library(cs2_internal SHARED
    dllmain.cpp
    hooks/present.cpp
    features/esp.cpp
    features/aimbot.cpp
    features/fov.cpp
    features/chams.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(cs2_internal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master
    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui-master/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/features
)

target_link_libraries(cs2_internal PRIVATE
    d3d11
    dxgi
    minhook
)

set_target_properties(cs2_internal PROPERTIES
    OUTPUT_NAME "cs2_internal"
    SUFFIX ".dll"
    PREFIX ""
)

target_link_options(cs2_internal PRIVATE $<$<CONFIG:Release>:/DEBUG>)
