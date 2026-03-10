cmake_minimum_required(VERSION 3.20)
project(cellwright LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/Dependencies.cmake)

# ── Main executable ──────────────────────────────────────────────────────────
file(GLOB_RECURSE DASHBOARD_SOURCES
    src/main.cpp
    src/app/*.cpp
    src/core/*.cpp
    src/formula/*.cpp
    src/plugin/*.cpp
    src/builtin/*.cpp
    src/io/*.cpp
    src/ui/*.cpp
)

add_executable(cellwright ${DASHBOARD_SOURCES})

target_include_directories(cellwright PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Expose ImGui's bundled font directory so we can load Roboto at runtime
target_compile_definitions(cellwright PRIVATE
    IMGUI_FONT_DIR="${imgui_SOURCE_DIR}/misc/fonts"
)

# SIMD flags for x86_64 (ARM NEON is auto-enabled on Apple Silicon)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(cellwright PRIVATE -mavx2 -mfma)
endif()

target_link_libraries(cellwright PRIVATE
    plugin_arch
    imgui_glfw_opengl3
    implot_lib
    glfw
    pybind11::embed
    duckdb
)

if(APPLE)
    target_link_libraries(cellwright PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX)
    target_link_libraries(cellwright PRIVATE GL dl)
endif()

# ── Plugins ──────────────────────────────────────────────────────────────────
add_subdirectory(plugins/example_stats)
add_subdirectory(plugins/scientific)
add_subdirectory(plugins/bond)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
