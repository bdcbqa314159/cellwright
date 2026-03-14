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
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
endif()
FetchContent_MakeAvailable(plugin_arch)

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

# ── Dear ImGui (docking branch) ─────────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# Build ImGui as a static library with GLFW + OpenGL3 backends
add_library(imgui_glfw_opengl3 STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_glfw_opengl3 PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_glfw_opengl3 PUBLIC glfw)

if(APPLE)
    target_link_libraries(imgui_glfw_opengl3 PUBLIC "-framework OpenGL")
elseif(WIN32)
    target_link_libraries(imgui_glfw_opengl3 PUBLIC opengl32)
elseif(UNIX)
    target_link_libraries(imgui_glfw_opengl3 PUBLIC GL)
endif()

# ── ImPlot ───────────────────────────────────────────────────────────────────
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(implot)

add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_glfw_opengl3)

# ── Python + pybind11 (for Python plugin adapter) ───────────────────────────
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
    GIT_TAG        v1.1.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(duckdb)

# ── NFD (native file dialogs) ────────────────────────────────────────────────
set(NFD_PORTAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        v1.2.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nfd)

# ── Google Test ──────────────────────────────────────────────────────────────
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
