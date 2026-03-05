#pragma once
#include "core/Workbook.hpp"
#include "core/Command.hpp"
#include "core/Clipboard.hpp"
#include "core/CellFormat.hpp"
#include "formula/FunctionRegistry.hpp"
#include "formula/DependencyGraph.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "plugin/PluginManager.hpp"
#include "ui/MainWindow.hpp"

namespace magic {

struct PerSheetState {
    UndoManager undo_manager;
    FormatMap format_map;
    DependencyGraph dep_graph;
};

struct AppState {
    Workbook workbook;
    FunctionRegistry function_registry;
    PluginManager plugin_manager{function_registry};
    MainWindow main_window;
    AsyncRecalcEngine async_recalc;
    Clipboard clipboard;
    std::string current_file;  // path to current .magic file (empty = untitled)
    std::string pending_plugin_path;  // set by DropHandler when untrusted; cleared by modal

    // Per-sheet state (indexed by sheet index, kept in sync with workbook)
    std::vector<PerSheetState> sheet_states{1};

    PerSheetState& active_state() { return sheet_states[workbook.active_index()]; }
    const PerSheetState& active_state() const { return sheet_states[workbook.active_index()]; }

    void ensure_sheet_states() {
        while (static_cast<int>(sheet_states.size()) < workbook.sheet_count())
            sheet_states.emplace_back();
    }
};

}  // namespace magic
