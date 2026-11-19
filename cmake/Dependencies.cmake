include(FetchContent)

# ── plugin_arch ──────────────────────────────────────────────────────────────
# Use local sibling repo if available, otherwise fetch from GitHub.
set(_plugin_arch_local "${CMAKE_CURRENT_SOURCE_DIR}/../cpp_plugin_arch")
if(EXISTS "${_plugin_arch_local}/CMakeLists.txt")
    FetchContent_Declare(plugin_arch
        SOURCE_DIR ${_plugin_arch_local}
    )
else()
    FetchContent_Declare(plugin_arch
        GIT_REPOSITORY https://github.com/bdcbqa314159/cpp_plugin_arch.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
endif()
FetchContent_MakeAvailable(plugin_arch)

# ── Dear ImGui core (docking branch) ─────────────────────────────────────────
# Fetched and built UNCONDITIONALLY. ImGui's core is portable C++ with no platform
# dependency: it needs a backend only to see a window and draw pixels. Keeping it
# outside the CELLWRIGHT_BUILD_APP guard is what lets ui/ be compiled and tested
# headlessly, including under the gcc preset (specs/03_headless_ui_tests.md).
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# imgui_demo.cpp is deliberately absent: nothing calls ShowDemoWindow and it is the
# largest single translation unit in ImGui.
add_library(imgui_core STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

target_include_directories(imgui_core SYSTEM PUBLIC ${imgui_SOURCE_DIR})

# ImPlot is portable C++ — it draws through ImGui's draw lists and includes only
# <immintrin.h> and <stdlib.h> (verified). Built unconditionally alongside imgui_core
# so ChartPanel can live in cellwright_ui and compile headlessly.
# ── ImPlot ───────────────────────────────────────────────────────────────────
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(implot)

# ImPlot needs only ImGui core; it draws through ImGui's draw lists.
add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

target_include_directories(implot_lib SYSTEM PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_core)
# GUI-only dependencies. Skipped when CELLWRIGHT_BUILD_APP=OFF so a core+tests build
# needs no windowing stack — notably GLFW's Objective-C sources, which GCC cannot
# compile on macOS.
if(CELLWRIGHT_BUILD_APP)

    # ── GLFW ─────────────────────────────────────────────────────────────────────
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw)

    # ── ImGui platform/renderer backends ────────────────────────────────────────
    add_library(imgui_backends STATIC
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(imgui_backends SYSTEM PUBLIC ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui_backends PUBLIC imgui_core glfw)

    if(APPLE)
        target_link_libraries(imgui_backends PUBLIC "-framework OpenGL")
    elseif(WIN32)
        target_link_libraries(imgui_backends PUBLIC opengl32)
    elseif(UNIX)
        target_link_libraries(imgui_backends PUBLIC GL)
    endif()

endif()

# ── Python + pybind11 (for Python plugin adapter) ───────────────────────────
# Prefer a non-framework interpreter. CMake defaults to Python3_FIND_FRAMEWORK=FIRST
# on macOS, which selects Xcode's Python3.framework; its libpython install name is
# @rpath/Python3.framework/Versions/3.9/Python3 while the emitted rpath points at
# .../Versions/3.9/lib, so the link succeeds but the binary aborts on exec.
set(Python3_FIND_FRAMEWORK LAST)

# Make pybind11 use CMake's FindPython result instead of its deprecated
# FindPythonLibsNew.cmake, which resolves Python independently and would drag the
# framework libpython back onto the link line regardless of the setting above.
set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pybind11)

# ── DuckDB ───────────────────────────────────────────────────────────────────
set(BUILD_SHELL OFF CACHE BOOL "" FORCE)
set(BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(duckdb
    GIT_REPOSITORY https://github.com/duckdb/duckdb.git
    GIT_TAG        v1.5.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(duckdb)

# DuckDB declares its public headers with directory-scoped include_directories(), which
# does not propagate to consumers through target_link_libraries. Attach it to the target
# so anything linking `duckdb` can find duckdb.hpp without repeating the path.
# Wrapped in BUILD_INTERFACE because duckdb carries install/export rules, and CMake
# rejects a bare in-tree path in an exported target's INTERFACE_INCLUDE_DIRECTORIES.
# SYSTEM so -Wall/-Wextra on our targets does not report ~141 -Wunused-parameter
# warnings from DuckDB's own headers, which we neither own nor can fix.
target_include_directories(duckdb
    SYSTEM INTERFACE $<BUILD_INTERFACE:${duckdb_SOURCE_DIR}/src/include>)

if(CELLWRIGHT_BUILD_APP)

    # ── NFD (native file dialogs) ────────────────────────────────────────────────
    set(NFD_PORTAL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG        v1.2.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nfd)

endif()

# ── Google Test ──────────────────────────────────────────────────────────────
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
