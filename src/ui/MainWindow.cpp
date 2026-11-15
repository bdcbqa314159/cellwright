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

void MainWindow::render(SessionState& state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_MenuBar;

    ImGui::Begin("CellWright", nullptr, window_flags);

    render_menu_bar(state);
    handle_keyboard(state);

    auto& ci = state.cell_input;
    auto& as = state.active_state();

    // Ctrl+Scroll to zoom, Ctrl+0 to reset
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.WantTextInput) {
        if (io.MouseWheel != 0.0f) {
            zoom_ += io.MouseWheel * 0.1f;
            zoom_ = std::clamp(zoom_, 0.5f, 3.0f);
            io.MouseWheel = 0.0f;  // consume so grid doesn't scroll
        }
        if (ImGui::IsKeyPressed(ImGuiKey_0)) {
            zoom_ = 1.0f;
        }
    }
    io.FontGlobalScale = zoom_;

    // Apply any async recalc results
    auto async_results = state.async_recalc.poll_results();
    if (!async_results.empty()) {
        for (const auto& r : async_results) {
            state.workbook.active_sheet().set_value(r.addr, r.value);
        }
        // Force formula bar to refresh so it shows updated values (#30)
        formula_bar_.force_refresh();
    }

    Sheet& sheet = state.workbook.active_sheet();

    // Formula bar
    bool cell_editing = grid_state_.editor.is_editing();
    const char* editor_buf = cell_editing ? grid_state_.editor.buffer() : nullptr;
    if (formula_bar_.render(sheet, grid_state_.selection.selected_cell, cell_editing,
                            &state.function_registry, mono_font_, editor_buf)) {
        ci.process(formula_bar_.buffer(), sheet, grid_state_.selection.selected_cell,
                   as.undo_manager, as.format_map, as.dep_graph, state.workbook);
    }
    // If formula bar took focus from cell editor, cancel the cell editor
    // so the user can continue editing in the formula bar.
    if (formula_bar_.took_focus()) {
        formula_bar_.clear_took_focus();
        grid_state_.editor.cancel();
    }
    if (formula_bar_.has_nav_target()) {
        grid_state_.selection.selected_cell = formula_bar_.consume_nav_target();
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }

    // Find bar (below formula bar)
    find_bar_.render(sheet, &as.undo_manager);
    if (find_bar_.has_nav_target()) {
        grid_state_.selection.selected_cell = find_bar_.consume_nav_target();
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }

    ImGui::Separator();

    // Pass formula bar to grid for reference highlighting and click-to-insert
    grid_state_.formula_bar_buf = formula_bar_.is_formula_mode()
        ? formula_bar_.buffer() : nullptr;
    grid_state_.formula_bar = &formula_bar_;
    grid_state_.dark_theme = (theme_ == Theme::Dark);
    grid_state_.registry = &state.function_registry;
    grid_state_.mono_font = mono_font_;
    grid_state_.cond_format = &as.cond_format;
    grid_state_.row_filter = &as.row_filter;
    grid_state_.find_matches = find_bar_.is_visible() ? &find_bar_.matches() : nullptr;
    grid_state_.find_match_index = find_bar_.current_match_index();

    // Splash / empty state hint
    bool show_splash = state.current_file.empty() && !state.is_dirty();
    if (show_splash) {
        // Check if all cells are empty
        bool all_empty = true;
        for (int32_t c = 0; c < sheet.col_count() && all_empty; ++c) {
            if (sheet.column(c).size() > 0)
                all_empty = false;
        }
        if (all_empty) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 text_size = ImGui::CalcTextSize("Start typing, or open a file with Ctrl+O");
            ImGui::SetCursorPos(ImVec2(
                ImGui::GetCursorPosX() + (avail.x - text_size.x) * 0.5f,
                ImGui::GetCursorPosY() + avail.y * 0.35f));
            ImGui::TextDisabled("Start typing, or open a file with Ctrl+O");
        }
    }

    // Spreadsheet grid
    if (grid_.render(sheet, grid_state_, as.format_map)) {
        ci.process(grid_state_.editor.buffer(), sheet,
                   grid_state_.editor.editing_cell(),
                   as.undo_manager, as.format_map, as.dep_graph, state.workbook);
    }

    dispatch_context_action(state, sheet);
    handle_drag_completion(state, sheet);
    render_sheet_tabs(state);
    render_status_bar(state, sheet);

    ImGui::End();

    // Render toast notifications
    toasts_.render();

    render_modals(state);

    // Render conditional formatting editor window
    if (show_cond_format_) render_cond_format_editor(state);

    // Render chart panel as a separate dockable window
    chart_panel_.render(sheet);

    // Render SQL query panel
    sql_panel_.render(state.duckdb_engine, sheet);

    // Initialize any new plugin panels, then render them
    state.plugin_manager.init_panels(state.workbook);
    state.plugin_manager.render_panels();
}

// ── Decomposed render sub-methods ───────────────────────────────────────────

void MainWindow::dispatch_context_action(SessionState& state, Sheet& sheet) {
    if (grid_state_.context_action == ContextAction::None) return;

    auto action = grid_state_.context_action;
    grid_state_.context_action = ContextAction::None;
    auto& ci = state.cell_input;
    auto& as = state.active_state();

    switch (action) {
        case ContextAction::Cut:
            action_cut(state);
            break;
        case ContextAction::Copy:
            action_copy(state);
            break;
        case ContextAction::Paste:
            action_paste(state);
            break;
        case ContextAction::Clear:
            ci.process("", sheet, grid_state_.selection.selected_cell,
                       as.undo_manager, as.format_map, as.dep_graph, state.workbook);
            break;
        case ContextAction::InsertRow: {
            auto cmd = std::make_unique<InsertRowCommand>(grid_state_.selection.selected_cell.row);
            as.undo_manager.execute(std::move(cmd), sheet);
            as.dep_graph.shift_rows(grid_state_.selection.selected_cell.row, 1);
            as.format_map.shift_rows(grid_state_.selection.selected_cell.row, 1);
            break;
        }
        case ContextAction::InsertCol: {
            auto cmd = std::make_unique<InsertColumnCommand>(grid_state_.selection.selected_cell.col);
            as.undo_manager.execute(std::move(cmd), sheet);
            as.dep_graph.shift_cols(grid_state_.selection.selected_cell.col, 1);
            as.format_map.shift_cols(grid_state_.selection.selected_cell.col, 1);
            break;
        }
        case ContextAction::DeleteRow: {
            auto cmd = std::make_unique<DeleteRowCommand>(grid_state_.selection.selected_cell.row, sheet);
            as.undo_manager.execute(std::move(cmd), sheet);
            as.dep_graph.shift_rows(grid_state_.selection.selected_cell.row, -1);
            as.format_map.shift_rows(grid_state_.selection.selected_cell.row, -1);
            break;
        }
        case ContextAction::DeleteCol: {
            auto cmd = std::make_unique<DeleteColumnCommand>(grid_state_.selection.selected_cell.col, sheet);
            as.undo_manager.execute(std::move(cmd), sheet);
            as.dep_graph.shift_cols(grid_state_.selection.selected_cell.col, -1);
            as.format_map.shift_cols(grid_state_.selection.selected_cell.col, -1);
            break;
        }
        case ContextAction::SortAsc:
        case ContextAction::SortDesc: {
            bool asc = (action == ContextAction::SortAsc);
            auto cmd = std::make_unique<SortColumnCommand>(grid_state_.context_col, asc, sheet);
            as.undo_manager.execute(std::move(cmd), sheet);
            std::unordered_set<CellAddress> dirty;
            for (auto& [addr, _] : sheet.all_formulas())
                dirty.insert(addr);
            if (!dirty.empty())
                ci.batch_recalc(sheet, dirty, as.dep_graph, &state.workbook);
            break;
        }
        default: break;
    }
}

void MainWindow::handle_drag_completion(SessionState& state, Sheet& sheet) {
    if (!grid_state_.drag.drag_completed) return;

    grid_state_.drag.drag_completed = false;
    CellAddress src = grid_state_.drag.drag_source;
    CellAddress tgt = grid_state_.drag.drag_target;
    auto& ci = state.cell_input;
    auto& as = state.active_state();

    if (grid_state_.drag.drag_mode == CellDragMode::Move && !(src == tgt)) {
        CellValue src_val = sheet.get_value(src);
        std::string src_formula = sheet.get_formula(src);

        if (!src_formula.empty()) {
            int32_t dcol = tgt.col - src.col;
            int32_t drow = tgt.row - src.row;
            std::string adjusted = Clipboard::adjust_references(src_formula, dcol, drow);
            ci.process(("=" + adjusted).c_str(), sheet, tgt,
                       as.undo_manager, as.format_map, as.dep_graph, state.workbook);
        } else {
            std::string display = to_display_string(src_val);
            ci.process(display.c_str(), sheet, tgt,
                       as.undo_manager, as.format_map, as.dep_graph, state.workbook);
        }
        ci.process("", sheet, src,
                   as.undo_manager, as.format_map, as.dep_graph, state.workbook);
        grid_state_.selection.selected_cell = tgt;
        grid_state_.selection.sel_anchor = tgt;
        grid_state_.selection.has_range = false;
    } else if (grid_state_.drag.drag_mode == CellDragMode::Fill && !(src == tgt)) {
        CellValue src_val = sheet.get_value(src);
        std::string src_formula = sheet.get_formula(src);

        int32_t c1 = std::min(src.col, tgt.col);
        int32_t c2 = std::max(src.col, tgt.col);
        int32_t r1 = std::min(src.row, tgt.row);
        int32_t r2 = std::max(src.row, tgt.row);

        // Detect fill direction and pattern for non-formula numeric cells
        bool vertical = (r2 - r1) >= (c2 - c1);
        FillPattern pattern;
        bool use_pattern = src_formula.empty() && is_number(src_val);
        if (use_pattern)
            pattern = detect_pattern(sheet, src, vertical);

        std::unordered_set<CellAddress> changed_cells;
        for (int32_t c = c1; c <= c2; ++c) {
            for (int32_t r = r1; r <= r2; ++r) {
                if (c == src.col && r == src.row) continue;
                CellAddress fill_addr{c, r};
                // Distance along the fill axis (not a flat counter)
                int axis_dist = vertical ? (r - src.row) : (c - src.col);
                if (axis_dist == 0) axis_dist = vertical ? (c - src.col) : (r - src.row);
                if (axis_dist < 0) axis_dist = -axis_dist;
                if (!src_formula.empty()) {
                    std::string adjusted = Clipboard::adjust_references(
                        src_formula, c - src.col, r - src.row);
                    ci.process_no_recalc(("=" + adjusted).c_str(), sheet, fill_addr,
                                         as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                } else if (use_pattern && pattern.kind == FillPattern::Kind::Arithmetic) {
                    CellValue fv = fill_value(pattern, axis_dist);
                    std::string display = to_display_string(fv);
                    ci.process_no_recalc(display.c_str(), sheet, fill_addr,
                                         as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                } else {
                    std::string display = to_display_string(src_val);
                    ci.process_no_recalc(display.c_str(), sheet, fill_addr,
                                         as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                }
                changed_cells.insert(fill_addr);
            }
        }
        ci.batch_recalc(sheet, changed_cells, as.dep_graph, &state.workbook);
    }
    grid_state_.drag.drag_mode = CellDragMode::None;
}

}  // namespace magic
