#pragma once
#include "core/Workbook.hpp"
#include "core/Command.hpp"
#include "core/Clipboard.hpp"
#include "core/CellFormat.hpp"
#include "formula/FunctionRegistry.hpp"
#include "formula/DependencyGraph.hpp"
#include "plugin/PluginManager.hpp"
#include "ui/MainWindow.hpp"

namespace magic {

struct AppState {
    Workbook workbook;
    FunctionRegistry function_registry;
    PluginManager plugin_manager{function_registry};
    MainWindow main_window;
    UndoManager undo_manager;
    Clipboard clipboard;
    FormatMap format_map;
    DependencyGraph dep_graph;
    std::string current_file;  // path to current .magic file (empty = untitled)
};

}  // namespace magic
