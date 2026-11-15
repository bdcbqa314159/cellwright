// Part of MainWindow, split out of MainWindow.cpp purely to keep each translation
// unit readable (issue #17). Same class, same behaviour, and no declaration in
// MainWindow.hpp changed. Command actions shared by the menu and keyboard paths, plus the file
// dialogs and the clipboard marching-ants state.

#include "ui/MainWindow.hpp"
#include "session/SessionState.hpp"
#include "session/AutoSave.hpp"
#include "core/ConditionalFormat.hpp"
#include "core/FillPattern.hpp"
#include "core/Workbook.hpp"
#include "services/Clipboard.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "util/Sha256.hpp"
#include <nfd.hpp>
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

// ── Shared action methods ────────────────────────────────────────────────────

void MainWindow::action_save(SessionState& state) {
    if (!state.current_file.empty()) {
        if (WorkbookIO::save(state.current_file, state.workbook)) {
            state.mark_saved();
            toasts_.show("Saved");
        } else {
            toasts_.show("Save failed!");
        }
    } else {
        do_save(state);
    }
}

void MainWindow::action_undo(SessionState& state) {
    auto& as = state.active_state();
    auto& sheet = state.workbook.active_sheet();
    as.undo_manager.undo(sheet);
    if (auto cell = as.undo_manager.last_affected())
        state.cell_input.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
}

void MainWindow::action_redo(SessionState& state) {
    auto& as = state.active_state();
    auto& sheet = state.workbook.active_sheet();
    as.undo_manager.redo(sheet);
    if (auto cell = as.undo_manager.last_affected())
        state.cell_input.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
}

void MainWindow::action_copy(SessionState& state) {
    state.cell_input.copy(state.clipboard, state.workbook.active_sheet(),
                          grid_state_.selection.selected_cell, grid_state_.selection.has_range,
                          grid_state_.sel_min(), grid_state_.sel_max());
    update_marching_ants(state.clipboard);
}

void MainWindow::action_cut(SessionState& state) {
    state.cell_input.cut(state.clipboard, state.workbook.active_sheet(),
                         grid_state_.selection.selected_cell, grid_state_.selection.has_range,
                         grid_state_.sel_min(), grid_state_.sel_max());
    update_marching_ants(state.clipboard);
}

void MainWindow::action_paste(SessionState& state) {
    if (state.clipboard.has_data()) {
        auto& as = state.active_state();
        state.cell_input.paste(state.clipboard, state.workbook.active_sheet(),
                               grid_state_.selection.selected_cell, as.undo_manager, as.format_map,
                               as.dep_graph, state.workbook);
        clear_marching_ants();
    }
}

void MainWindow::action_paste_special(SessionState& state, PasteMode mode) {
    if (state.clipboard.has_data()) {
        auto& as = state.active_state();
        state.cell_input.paste_special(state.clipboard, state.workbook.active_sheet(),
                                       grid_state_.selection.selected_cell, as.undo_manager, as.format_map,
                                       as.dep_graph, state.workbook, mode);
        clear_marching_ants();
    }
}

// ── Menu bar ────────────────────────────────────────────────────────────────

void MainWindow::update_marching_ants(const Clipboard& clip) {
    if (!clip.has_data()) {
        clear_marching_ants();
        return;
    }
    auto cells = clip.source_cells();
    if (cells.empty()) {
        clear_marching_ants();
        return;
    }
    CellAddress mn = cells[0], mx = cells[0];
    for (const auto& c : cells) {
        mn.col = std::min(mn.col, c.col);
        mn.row = std::min(mn.row, c.row);
        mx.col = std::max(mx.col, c.col);
        mx.row = std::max(mx.row, c.row);
    }
    grid_state_.clipboard_visual.show_marching_ants = true;
    grid_state_.clipboard_visual.clip_min = mn;
    grid_state_.clipboard_visual.clip_max = mx;
}

void MainWindow::clear_marching_ants() {
    grid_state_.clipboard_visual.show_marching_ants = false;
}

// ── Native file dialogs (NFD) ───────────────────────────────────────────────
// NOTE: NFD calls are hardwired here. An abstraction layer (e.g. IFileDialog)
// would be needed to unit-test file operations without native dialogs.

void MainWindow::do_open(SessionState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CellWright", "magic"}, {"All Files", "*"}};
    if (NFD::OpenDialog(path, filters, 2) == NFD_OKAY) {
        if (!state.open_file(path.get()))
            toasts_.show("Failed to open file");
    }
}

void MainWindow::do_save(SessionState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CellWright", "magic"}};
    std::string default_name;
    if (!state.current_file.empty())
        default_name = std::filesystem::path(state.current_file).filename().string();
    if (NFD::SaveDialog(path, filters, 1, nullptr,
                        default_name.empty() ? nullptr : default_name.c_str()) == NFD_OKAY) {
        if (state.save_file(path.get()))
            toasts_.show("Saved");
    }
}

void MainWindow::do_import_csv(SessionState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CSV", "csv"}, {"All Files", "*"}};
    if (NFD::OpenDialog(path, filters, 2) == NFD_OKAY) {
        if (state.import_csv(path.get()))
            toasts_.show("CSV imported");
        else
            toasts_.show("Failed to import CSV");
    }
}

void MainWindow::do_export_csv(SessionState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CSV", "csv"}};
    if (NFD::SaveDialog(path, filters, 1) == NFD_OKAY) {
        if (state.export_csv(path.get()))
            toasts_.show("CSV exported");
        else
            toasts_.show("Failed to export CSV");
    }
}

}  // namespace magic
