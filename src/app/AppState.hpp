#pragma once
#include "core/Workbook.hpp"
#include "core/Command.hpp"
#include "io/WorkbookIO.hpp"
#include "core/Clipboard.hpp"
#include "core/CellFormat.hpp"
#include "core/DuckDBEngine.hpp"
#include "core/CellInputService.hpp"
#include "formula/FunctionRegistry.hpp"
#include "formula/DependencyGraph.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "plugin/PluginManager.hpp"
#include "ui/MainWindow.hpp"
#include "ui/ToastManager.hpp"

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
    CellInputService cell_input{function_registry};
    MainWindow main_window;
    AsyncRecalcEngine async_recalc;
    Clipboard clipboard;
    ToastManager toasts;
    DuckDBEngine duckdb_engine;
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

    // Reset to a fresh untitled workbook
    void reset_to_new() {
        workbook = Workbook();
        sheet_states.clear();
        sheet_states.emplace_back();
        saved_generations_ = {0};
        current_file.clear();
    }

    // Open a file, replacing current workbook state
    bool open_file(const std::string& path) {
        if (WorkbookIO::load(path, workbook)) {
            current_file = path;
            sheet_states.clear();
            sheet_states.resize(workbook.sheet_count());
            mark_saved();
            return true;
        }
        return false;
    }

    // Dirty tracking: compare each sheet's undo generation against saved snapshot
    bool is_dirty() const {
        if (sheet_states.size() != saved_generations_.size()) return true;
        for (size_t i = 0; i < sheet_states.size(); ++i) {
            if (sheet_states[i].undo_manager.generation() != saved_generations_[i])
                return true;
        }
        return false;
    }

    void mark_saved() {
        saved_generations_.resize(sheet_states.size());
        for (size_t i = 0; i < sheet_states.size(); ++i)
            saved_generations_[i] = sheet_states[i].undo_manager.generation();
    }

private:
    std::vector<uint64_t> saved_generations_{0};  // one per sheet
};

}  // namespace magic
