// Part of MainWindow, split out of MainWindow.cpp purely to keep each translation
// unit readable (issue #17). Same class, same behaviour, and no declaration in
// MainWindow.hpp changed. Keyboard routing: shortcuts and grid navigation.

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <nfd.hpp>

#include "core/ConditionalFormat.hpp"
#include "core/FillPattern.hpp"
#include "core/Workbook.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "services/Clipboard.hpp"
#include "session/AutoSave.hpp"
#include "session/SessionState.hpp"
#include "ui/MainWindow.hpp"
#include "util/Sha256.hpp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

// ── Shared action methods ────────────────────────────────────────────────────

void MainWindow::handle_keyboard(SessionState& state) {
    if (ImGui::GetIO().WantTextInput) return;
    handle_shortcuts(state);
    handle_navigation(state);
}

void MainWindow::handle_shortcuts(SessionState& state) {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        action_undo(state);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        action_redo(state);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        action_copy(state);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        action_cut(state);
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_V) && state.clipboard.has_data()) {
        show_paste_special_ = true;
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V) && state.clipboard.has_data()) {
        action_paste(state);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        action_save(state);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        do_open(state);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
        formula_bar_.focus_name_box();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
        find_bar_.show_find();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_H)) {
        find_bar_.show_replace();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (find_bar_.is_visible()) find_bar_.hide();
        clear_marching_ants();
    }
}

void MainWindow::handle_navigation(SessionState& state) {
    if (grid_state_.editor.is_editing()) return;

    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    auto& ci = state.cell_input;
    auto& as = state.active_state();

    // Jump to data boundary for Ctrl+Arrow navigation
    auto find_data_boundary = [&](const Sheet& s, int32_t col, int32_t row, int dc,
                                  int dr) -> CellAddress {
        bool cur_empty = is_empty(s.get_value({col, row}));
        int32_t max_col = std::max(0, std::min(s.col_count(), 256) - 1);
        int32_t max_row = std::max(0, s.row_count() - 1);
        int32_t c = col + dc, r = row + dr;
        if (cur_empty) {
            while (c >= 0 && r >= 0 && c <= max_col && r <= max_row) {
                if (!is_empty(s.get_value({c, r}))) return {c, r};
                c += dc;
                r += dr;
            }
            return {std::clamp(dc > 0 ? max_col : 0, 0, max_col),
                    std::clamp(dr > 0 ? max_row : 0, 0, max_row)};
        } else {
            while (c >= 0 && r >= 0 && c <= max_col && r <= max_row) {
                if (is_empty(s.get_value({c, r}))) return {c - dc, r - dr};
                c += dc;
                r += dr;
            }
            return {std::clamp(c - dc, 0, max_col), std::clamp(r - dr, 0, max_row)};
        }
    };

    // Arrow key navigation (Shift extends selection)
    auto move_cursor = [&](int dc, int dr) {
        if (!shift) {
            grid_state_.selection.has_range = false;
            grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        } else if (!grid_state_.selection.has_range) {
            grid_state_.selection.has_range = true;
            grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        }
        if (ctrl) {
            auto& s = state.workbook.active_sheet();
            auto dest = find_data_boundary(s, grid_state_.selection.selected_cell.col,
                                           grid_state_.selection.selected_cell.row, dc, dr);
            grid_state_.selection.selected_cell = dest;
        } else {
            grid_state_.selection.selected_cell.col =
                std::max(0, grid_state_.selection.selected_cell.col + dc);
            grid_state_.selection.selected_cell.row =
                std::max(0, grid_state_.selection.selected_cell.row + dr);
        }
    };

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) move_cursor(0, -1);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) move_cursor(0, 1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move_cursor(-1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move_cursor(1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        grid_state_.selection.has_range = false;
        grid_state_.selection.selected_cell.col++;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        grid_state_.selection.has_range = false;
        grid_state_.selection.selected_cell.row++;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        auto& sheet = state.workbook.active_sheet();
        if (grid_state_.selection.has_range) {
            auto smin = grid_state_.sel_min();
            auto smax = grid_state_.sel_max();
            std::unordered_set<CellAddress> changed;
            for (int32_t c = smin.col; c <= smax.col; ++c) {
                for (int32_t r = smin.row; r <= smax.row; ++r) {
                    CellAddress addr{c, r};
                    ci.process_no_recalc("", sheet, addr, as.undo_manager, as.format_map,
                                         as.dep_graph, state.workbook);
                    changed.insert(addr);
                }
            }
            ci.batch_recalc(sheet, changed, as.dep_graph, &state.workbook);
        } else {
            ci.process("", sheet, grid_state_.selection.selected_cell, as.undo_manager,
                       as.format_map, as.dep_graph, state.workbook);
        }
    }

    // Ctrl+Home → go to A1, Ctrl+End → go to last used cell
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Home)) {
        grid_state_.selection.selected_cell = {0, 0};
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_End)) {
        auto& s = state.workbook.active_sheet();
        int32_t max_col = 0, max_row = 0;
        for (int32_t c = 0; c < s.col_count(); ++c) {
            int32_t sz = s.column(c).size();
            if (sz > 0) {
                max_col = std::max(max_col, c);
                max_row = std::max(max_row, sz - 1);
            }
        }
        grid_state_.selection.selected_cell = {max_col, max_row};
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }

    // Page Up / Page Down
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        int visible_rows = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().y /
                                                        ImGui::GetTextLineHeightWithSpacing()));
        grid_state_.selection.selected_cell.row =
            std::max(0, grid_state_.selection.selected_cell.row - visible_rows);
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        int visible_rows = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().y /
                                                        ImGui::GetTextLineHeightWithSpacing()));
        grid_state_.selection.selected_cell.row += visible_rows;
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }

    // Ctrl+PgUp / Ctrl+PgDn — switch sheet tabs
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        int idx = state.workbook.active_index();
        if (idx > 0) {
            state.workbook.set_active(idx - 1);
            state.ensure_sheet_states();
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        int idx = state.workbook.active_index();
        if (idx + 1 < state.workbook.sheet_count()) {
            state.workbook.set_active(idx + 1);
            state.ensure_sheet_states();
        }
    }

    // F2 to edit current cell
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        auto& sheet = state.workbook.active_sheet();
        std::string initial;
        if (sheet.has_formula(grid_state_.selection.selected_cell))
            initial = "=" + sheet.get_formula(grid_state_.selection.selected_cell);
        else
            initial = to_display_string(sheet.get_value(grid_state_.selection.selected_cell));
        clear_marching_ants();
        grid_state_.editor.begin_edit(grid_state_.selection.selected_cell, initial);
    }

    // Start typing on selected cell -> begin editing
    if (!ctrl && !io.KeyAlt) {
        ImGuiIO& input_io = ImGui::GetIO();
        if (input_io.InputQueueCharacters.Size > 0) {
            ImWchar wch = input_io.InputQueueCharacters[0];
            if (wch >= 32 && wch < 127) {
                std::string seed(1, static_cast<char>(wch));
                input_io.InputQueueCharacters.erase(input_io.InputQueueCharacters.Data);
                clear_marching_ants();
                grid_state_.editor.begin_edit(grid_state_.selection.selected_cell, seed, false);
            }
        }
    }
}

// ── Main render ─────────────────────────────────────────────────────────────

}  // namespace magic
