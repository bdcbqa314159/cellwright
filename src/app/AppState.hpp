#pragma once
#include "core/Workbook.hpp"
#include "core/Command.hpp"
#include "core/Clipboard.hpp"
#include "core/CellFormat.hpp"
#include "core/DuckDBEngine.hpp"
#include "core/CellInputService.hpp"
#include "formula/FunctionRegistry.hpp"
#include "formula/DependencyGraph.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "plugin/PluginManager.hpp"
#include "app/AutoSave.hpp"
#include "app/Settings.hpp"
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
    Settings settings;
    std::string current_file;  // path to current .magic file (empty = untitled)
    std::string pending_plugin_path;  // set by DropHandler when untrusted; cleared by modal
    AutoSave auto_save;
    bool font_rebuild_needed = false;  // set by UI when font size changes
    bool show_recovery_modal = false;  // set on startup if recovery file found

    // Per-sheet state (indexed by sheet index, kept in sync with workbook)
    std::vector<PerSheetState> sheet_states{1};

    PerSheetState& active_state() { return sheet_states[workbook.active_index()]; }
    const PerSheetState& active_state() const { return sheet_states[workbook.active_index()]; }

    // Invariant: sheet_states.size() must equal workbook.sheet_count().
    void ensure_sheet_states() {
        while (static_cast<int>(sheet_states.size()) < workbook.sheet_count())
            sheet_states.emplace_back();
        while (static_cast<int>(sheet_states.size()) > workbook.sheet_count()) {
            sheet_states.pop_back();
            if (static_cast<int>(saved_generations_.size()) > workbook.sheet_count())
                saved_generations_.pop_back();
        }
    }

    void remove_sheet_state(int index) {
        if (index < 0 || index >= static_cast<int>(sheet_states.size())) return;
        sheet_states.erase(sheet_states.begin() + index);
        if (index < static_cast<int>(saved_generations_.size()))
            saved_generations_.erase(saved_generations_.begin() + index);
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
    [[nodiscard]] bool open_file(const std::string& path);

    // Save workbook to a path, updating current_file and marking saved
    [[nodiscard]] bool save_file(const std::string& path);

    // Import CSV into the active sheet, resetting per-sheet state
    [[nodiscard]] bool import_csv(const std::string& path);

    // Export active sheet to CSV
    [[nodiscard]] bool export_csv(const std::string& path);

    // Dirty tracking: compare each sheet's undo generation against saved snapshot
    [[nodiscard]] bool is_dirty() const {
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
