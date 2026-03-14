cmake_minimum_required(VERSION 3.20)

project(cellwright
    VERSION   0.1.0
    LANGUAGES CXX
    DESCRIPTION "Cross-platform quantitative spreadsheet"
)

# ── Global settings ──────────────────────────────────────────────

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── Options ──────────────────────────────────────────────────────

option(CELLWRIGHT_BUILD_PLUGINS "Build bundled plugins"  ON)
option(CELLWRIGHT_BUILD_TESTS   "Build test suite"       ON)

# ── Dependencies ─────────────────────────────────────────────────

include(cmake/Dependencies.cmake)

# ── Core static library ─────────────────────────────────────────
# Everything except main.cpp and UI code. Tests link against this
# to avoid recompiling sources.

add_library(cellwright_core STATIC
    # core
    src/core/Arena.cpp
    src/core/CellAddress.cpp
    src/core/CellFormat.cpp
    src/core/CellInputService.cpp
    src/core/CellValue.cpp
    src/core/Clipboard.cpp
    src/core/Column.cpp
    src/core/Command.cpp
    src/core/DateSerial.cpp
    src/core/DuckDBEngine.cpp
    src/core/FormulaAdjust.cpp
    src/core/Sheet.cpp
    src/core/SimdOps.cpp
    src/core/Workbook.cpp
    # formula
    src/formula/AsyncRecalcEngine.cpp
    src/formula/DependencyGraph.cpp
    src/formula/Evaluator.cpp
    src/formula/FunctionRegistry.cpp
    src/formula/Parser.cpp
    src/formula/Tokenizer.cpp
    # builtins
    src/builtin/LogicFunctions.cpp
    src/builtin/MathFunctions.cpp
    src/builtin/SqlFunction.cpp
    src/builtin/StatFunctions.cpp
    src/builtin/TextFunctions.cpp
    # io
    src/io/CsvIO.cpp
    src/io/WorkbookIO.cpp
    # plugin
    src/plugin/PluginAllowlist.cpp
    src/plugin/PluginManager.cpp
    src/plugin/PyFunctionPlugin.cpp
)

target_include_directories(cellwright_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(cellwright_core PRIVATE -mavx2 -mfma)
endif()

target_link_libraries(cellwright_core PUBLIC
    plugin_arch
    pybind11::embed
    duckdb
)

# ── Main executable ──────────────────────────────────────────────

add_executable(cellwright
    src/main.cpp
    # app
    src/app/App.cpp
    src/app/AppState.cpp
    src/app/AutoSave.cpp
    src/app/Settings.cpp
    # ui
    src/ui/AutocompletePopup.cpp
    src/ui/CellEditor.cpp
    src/ui/ChartPanel.cpp
    src/ui/FindBar.cpp
    src/ui/FormulaBar.cpp
    src/ui/MainWindow.cpp
    src/ui/SpreadsheetGrid.cpp
    src/ui/SqlPanel.cpp
    src/ui/StyleSetup.cpp
    src/ui/ToastManager.cpp
    # plugin (UI-facing)
    src/plugin/DropHandler.cpp
    # util
    src/util/stb_image_write_impl.cpp
)

target_compile_definitions(cellwright PRIVATE
    IMGUI_FONT_DIR="${imgui_SOURCE_DIR}/misc/fonts"
)

target_link_libraries(cellwright PRIVATE
    cellwright_core
    imgui_glfw_opengl3
    implot_lib
    nfd
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

# ── Plugins ──────────────────────────────────────────────────────

if(CELLWRIGHT_BUILD_PLUGINS)
    add_subdirectory(plugins)
endif()

# ── Tests ────────────────────────────────────────────────────────

if(CELLWRIGHT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
