cmake_minimum_required(VERSION 3.20)
project(magic_dashboard LANGUAGES CXX)

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

add_executable(magic_dashboard ${DASHBOARD_SOURCES})

target_include_directories(magic_dashboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(magic_dashboard PRIVATE
    plugin_arch
    imgui_glfw_opengl3
    implot_lib
    glfw
)

if(APPLE)
    target_link_libraries(magic_dashboard PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX)
    target_link_libraries(magic_dashboard PRIVATE GL dl)
endif()

# ── Example plugin ───────────────────────────────────────────────────────────
add_subdirectory(plugins/example_stats)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
