#include "ui/MainWindow.hpp"
#include "app/AppState.hpp"
#include "app/AutoSave.hpp"
#include "core/ConditionalFormat.hpp"
#include "core/FillPattern.hpp"
#include "core/Workbook.hpp"
#include "core/Clipboard.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "util/Sha256.hpp"
#include <nfd.hpp>
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <filesystem>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

// ── Shared action methods ────────────────────────────────────────────────────

void MainWindow::action_save(AppState& state) {
    if (!state.current_file.empty()) {
        if (WorkbookIO::save(state.current_file, state.workbook)) {
            state.mark_saved();
            state.toasts.show("Saved");
        } else {
            state.toasts.show("Save failed!");
        }
    } else {
        do_save(state);
    }
}

void MainWindow::action_undo(AppState& state) {
    auto& as = state.active_state();
    auto& sheet = state.workbook.active_sheet();
    as.undo_manager.undo(sheet);
    if (auto cell = as.undo_manager.last_affected())
        state.cell_input.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
}

void MainWindow::action_redo(AppState& state) {
    auto& as = state.active_state();
    auto& sheet = state.workbook.active_sheet();
    as.undo_manager.redo(sheet);
    if (auto cell = as.undo_manager.last_affected())
        state.cell_input.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
}

void MainWindow::action_copy(AppState& state) {
    state.cell_input.copy(state.clipboard, state.workbook.active_sheet(),
                          grid_state_.selection.selected_cell, grid_state_.selection.has_range,
                          grid_state_.sel_min(), grid_state_.sel_max());
    update_marching_ants(state.clipboard);
}

void MainWindow::action_cut(AppState& state) {
    state.cell_input.cut(state.clipboard, state.workbook.active_sheet(),
                         grid_state_.selection.selected_cell, grid_state_.selection.has_range,
                         grid_state_.sel_min(), grid_state_.sel_max());
    update_marching_ants(state.clipboard);
}

void MainWindow::action_paste(AppState& state) {
    if (state.clipboard.has_data()) {
        auto& as = state.active_state();
        state.cell_input.paste(state.clipboard, state.workbook.active_sheet(),
                               grid_state_.selection.selected_cell, as.undo_manager, as.format_map,
                               as.dep_graph, state.workbook);
        clear_marching_ants();
    }
}

void MainWindow::action_paste_special(AppState& state, PasteMode mode) {
    if (state.clipboard.has_data()) {
        auto& as = state.active_state();
        state.cell_input.paste_special(state.clipboard, state.workbook.active_sheet(),
                                       grid_state_.selection.selected_cell, as.undo_manager, as.format_map,
                                       as.dep_graph, state.workbook, mode);
        clear_marching_ants();
    }
}

// ── Menu bar ────────────────────────────────────────────────────────────────

void MainWindow::render_menu_bar(AppState& state) {
    auto& ci = state.cell_input;
    auto& as = state.active_state();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                if (state.is_dirty()) {
                    show_dirty_new_modal_ = true;
                } else {
                    state.reset_to_new();
                }
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                do_open(state);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                action_save(state);
            }
            if (ImGui::MenuItem("Save As...")) {
                do_save(state);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Recent Files")) {
                auto& recent = state.settings.recent_files();
                if (recent.empty()) {
                    ImGui::MenuItem("(none)", nullptr, false, false);
                } else {
                    for (const auto& path : recent) {
                        auto fname = std::filesystem::path(path).filename().string();
                        if (ImGui::MenuItem(fname.c_str())) {
                            if (!state.open_file(path))
                                state.toasts.show("Failed to open file");
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import CSV...")) {
                do_import_csv(state);
            }
            if (ImGui::MenuItem("Export CSV...")) {
                do_export_csv(state);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            std::string undo_label = "Undo";
            if (as.undo_manager.can_undo())
                undo_label += " " + as.undo_manager.peek_undo_desc();
            std::string redo_label = "Redo";
            if (as.undo_manager.can_redo())
                redo_label += " " + as.undo_manager.peek_redo_desc();

            if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, as.undo_manager.can_undo())) {
                action_undo(state);
            }
            if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, as.undo_manager.can_redo())) {
                action_redo(state);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                action_copy(state);
            }
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                action_cut(state);
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, state.clipboard.has_data())) {
                action_paste(state);
            }
            if (ImGui::BeginMenu("Paste Special", state.clipboard.has_data())) {
                if (ImGui::MenuItem("Values Only"))
                    action_paste_special(state, PasteMode::ValuesOnly);
                if (ImGui::MenuItem("Formulas Only"))
                    action_paste_special(state, PasteMode::FormulasOnly);
                if (ImGui::MenuItem("Transpose"))
                    action_paste_special(state, PasteMode::Transpose);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Format")) {
            auto& addr = grid_state_.selection.selected_cell;
            CellFormat current = as.format_map.get(addr);

            auto apply_format = [&](CellFormat fmt) {
                if (grid_state_.selection.has_range) {
                    auto smin = grid_state_.sel_min();
                    auto smax = grid_state_.sel_max();
                    for (int32_t c = smin.col; c <= smax.col; ++c)
                        for (int32_t r = smin.row; r <= smax.row; ++r)
                            as.format_map.set({c, r}, fmt);
                } else {
                    as.format_map.set(addr, fmt);
                }
            };

            if (ImGui::MenuItem("General", nullptr, current.type == FormatType::GENERAL)) {
                apply_format({FormatType::GENERAL});
            }
            if (ImGui::MenuItem("Number (2 dp)", nullptr, current.type == FormatType::NUMBER)) {
                apply_format({FormatType::NUMBER, 2});
            }
            if (ImGui::MenuItem("Percentage", nullptr, current.type == FormatType::PERCENTAGE)) {
                apply_format({FormatType::PERCENTAGE, 1});
            }
            if (ImGui::MenuItem("Currency ($)", nullptr, current.type == FormatType::CURRENCY)) {
                apply_format({FormatType::CURRENCY, 2, "$"});
            }
            if (ImGui::MenuItem("Scientific", nullptr, current.type == FormatType::SCIENTIFIC)) {
                apply_format({FormatType::SCIENTIFIC, 2});
            }
            if (ImGui::MenuItem("Date (ISO)", nullptr, current.type == FormatType::DATE)) {
                apply_format({FormatType::DATE});
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Conditional Formatting...")) {
                show_cond_format_ = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            bool chart_visible = chart_panel_.is_visible();
            if (ImGui::MenuItem("Chart Panel", nullptr, chart_visible)) {
                chart_panel_.set_visible(!chart_visible);
            }
            bool sql_visible = sql_panel_.is_visible();
            if (ImGui::MenuItem("SQL Query Panel", nullptr, sql_visible)) {
                sql_panel_.set_visible(!sql_visible);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Dark", nullptr, theme_ == Theme::Dark)) {
                    theme_ = Theme::Dark;
                    apply_theme(theme_);
                }
                if (ImGui::MenuItem("Light", nullptr, theme_ == Theme::Light)) {
                    theme_ = Theme::Light;
                    apply_theme(theme_);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Freeze Panes")) {
                bool no_freeze = (grid_state_.freeze_rows == 0 && grid_state_.freeze_cols == 0);
                if (ImGui::MenuItem("None", nullptr, no_freeze)) {
                    grid_state_.freeze_rows = 0;
                    grid_state_.freeze_cols = 0;
                }
                if (ImGui::MenuItem("Freeze Top Row", nullptr,
                                    grid_state_.freeze_rows == 1 && grid_state_.freeze_cols == 0)) {
                    grid_state_.freeze_rows = 1;
                    grid_state_.freeze_cols = 0;
                }
                if (ImGui::MenuItem("Freeze First Column", nullptr,
                                    grid_state_.freeze_rows == 0 && grid_state_.freeze_cols == 1)) {
                    grid_state_.freeze_rows = 0;
                    grid_state_.freeze_cols = 1;
                }
                if (ImGui::MenuItem("Freeze at Selection")) {
                    auto& sel = grid_state_.selection.selected_cell;
                    grid_state_.freeze_rows = sel.row;
                    grid_state_.freeze_cols = sel.col;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Font Size")) {
                static constexpr float sizes[] = {12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 24.0f};
                for (float sz : sizes) {
                    char label[16];
                    snprintf(label, sizeof(label), "%.0fpt", sz);
                    if (ImGui::MenuItem(label, nullptr, state.settings.font_size == sz)) {
                        if (state.settings.font_size != sz) {
                            state.settings.font_size = sz;
                            state.font_rebuild_needed = true;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

}

// ── Keyboard shortcuts ──────────────────────────────────────────────────────

void MainWindow::handle_keyboard(AppState& state) {
    if (ImGui::GetIO().WantTextInput) return;
    handle_shortcuts(state);
    handle_navigation(state);
}

void MainWindow::handle_shortcuts(AppState& state) {
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
        if (find_bar_.is_visible())
            find_bar_.hide();
        clear_marching_ants();
    }
}

void MainWindow::handle_navigation(AppState& state) {
    if (grid_state_.editor.is_editing()) return;

    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;
    auto& ci = state.cell_input;
    auto& as = state.active_state();

    // Jump to data boundary for Ctrl+Arrow navigation
    auto find_data_boundary = [&](const Sheet& s, int32_t col, int32_t row, int dc, int dr) -> CellAddress {
        bool cur_empty = is_empty(s.get_value({col, row}));
        int32_t max_col = std::max(0, std::min(s.col_count(), 256) - 1);
        int32_t max_row = std::max(0, s.row_count() - 1);
        int32_t c = col + dc, r = row + dr;
        if (cur_empty) {
            while (c >= 0 && r >= 0 && c <= max_col && r <= max_row) {
                if (!is_empty(s.get_value({c, r}))) return {c, r};
                c += dc; r += dr;
            }
            return {std::clamp(dc > 0 ? max_col : 0, 0, max_col),
                    std::clamp(dr > 0 ? max_row : 0, 0, max_row)};
        } else {
            while (c >= 0 && r >= 0 && c <= max_col && r <= max_row) {
                if (is_empty(s.get_value({c, r}))) return {c - dc, r - dr};
                c += dc; r += dr;
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
            auto dest = find_data_boundary(s, grid_state_.selection.selected_cell.col, grid_state_.selection.selected_cell.row, dc, dr);
            grid_state_.selection.selected_cell = dest;
        } else {
            grid_state_.selection.selected_cell.col = std::max(0, grid_state_.selection.selected_cell.col + dc);
            grid_state_.selection.selected_cell.row = std::max(0, grid_state_.selection.selected_cell.row + dr);
        }
    };

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    move_cursor(0, -1);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))   move_cursor(0, 1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))   move_cursor(-1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))  move_cursor(1, 0);
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
                    ci.process_no_recalc("", sheet, addr,
                                         as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                    changed.insert(addr);
                }
            }
            ci.batch_recalc(sheet, changed, as.dep_graph, &state.workbook);
        } else {
            ci.process("", sheet, grid_state_.selection.selected_cell,
                       as.undo_manager, as.format_map, as.dep_graph, state.workbook);
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
        int visible_rows = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().y / ImGui::GetTextLineHeightWithSpacing()));
        grid_state_.selection.selected_cell.row = std::max(0, grid_state_.selection.selected_cell.row - visible_rows);
        grid_state_.selection.sel_anchor = grid_state_.selection.selected_cell;
        grid_state_.selection.has_range = false;
    }
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        int visible_rows = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().y / ImGui::GetTextLineHeightWithSpacing()));
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

void MainWindow::render(AppState& state) {
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
    if (formula_bar_.render(sheet, grid_state_.selection.selected_cell, cell_editing, &state.function_registry, state.mono_font)) {
        ci.process(formula_bar_.buffer(), sheet, grid_state_.selection.selected_cell,
                   as.undo_manager, as.format_map, as.dep_graph, state.workbook);
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

    // Pass formula bar buffer to grid for reference highlighting
    grid_state_.formula_bar_buf = formula_bar_.is_formula_mode()
        ? formula_bar_.buffer() : nullptr;
    grid_state_.dark_theme = (theme_ == Theme::Dark);
    grid_state_.registry = &state.function_registry;
    grid_state_.mono_font = state.mono_font;
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
    state.toasts.render();

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

void MainWindow::dispatch_context_action(AppState& state, Sheet& sheet) {
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

void MainWindow::handle_drag_completion(AppState& state, Sheet& sheet) {
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

        int step_counter = 0;
        std::unordered_set<CellAddress> changed_cells;
        for (int32_t c = c1; c <= c2; ++c) {
            for (int32_t r = r1; r <= r2; ++r) {
                if (c == src.col && r == src.row) continue;
                CellAddress fill_addr{c, r};
                ++step_counter;
                if (!src_formula.empty()) {
                    std::string adjusted = Clipboard::adjust_references(
                        src_formula, c - src.col, r - src.row);
                    ci.process_no_recalc(("=" + adjusted).c_str(), sheet, fill_addr,
                                         as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                } else if (use_pattern && pattern.kind == FillPattern::Kind::Arithmetic) {
                    CellValue fv = fill_value(pattern, step_counter);
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

void MainWindow::render_sheet_tabs(AppState& state) {
    if (ImGui::BeginTabBar("##sheets")) {
        for (int i = 0; i < state.workbook.sheet_count(); ++i) {
            bool selected = (i == state.workbook.active_index());
            if (ImGui::BeginTabItem(state.workbook.sheet(i).name().c_str(),
                                     nullptr,
                                     selected ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (!selected) state.workbook.set_active(i);
                ImGui::EndTabItem();
            }
        }
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            state.workbook.add_sheet();
            state.ensure_sheet_states();
        }
        ImGui::EndTabBar();
    }
}

void MainWindow::render_status_bar(AppState& state, Sheet& sheet) {
    ImGui::Separator();
    auto& as = state.active_state();
    auto fmt = as.format_map.get(grid_state_.selection.selected_cell);
    const char* fmt_name = "General";
    switch (fmt.type) {
        case FormatType::NUMBER: fmt_name = "Number"; break;
        case FormatType::PERCENTAGE: fmt_name = "%"; break;
        case FormatType::CURRENCY: fmt_name = "$"; break;
        case FormatType::SCIENTIFIC: fmt_name = "Sci"; break;
        case FormatType::DATE: fmt_name = "Date"; break;
        default: break;
    }

    if (grid_state_.selection.has_range) {
        auto smin = grid_state_.sel_min();
        auto smax = grid_state_.sel_max();
        uint64_t gen = sheet.value_generation();
        if (!(smin == cached_sel_min_) || !(smax == cached_sel_max_) || gen != cached_sel_gen_) {
            cached_sel_min_ = smin;
            cached_sel_max_ = smax;
            cached_sel_gen_ = gen;
            cached_sum_ = 0.0;
            cached_count_ = 0;
            for (int32_t c = smin.col; c <= smax.col; ++c) {
                for (int32_t r = smin.row; r <= smax.row; ++r) {
                    auto v = sheet.get_value({c, r});
                    if (is_number(v)) {
                        cached_sum_ += as_number(v);
                        ++cached_count_;
                    }
                }
            }
        }
        cached_has_range_ = true;
        int zoom_pct = static_cast<int>(zoom_ * 100.0f + 0.5f);
        if (cached_count_ > 0) {
            ImGui::Text("Cell: %s:%s  |  SUM: %.4g  AVG: %.4g  COUNT: %d  |  Format: %s  |  Zoom: %d%%",
                        smin.to_a1().c_str(), smax.to_a1().c_str(),
                        cached_sum_, cached_sum_ / cached_count_, cached_count_, fmt_name, zoom_pct);
        } else {
            ImGui::Text("Cell: %s:%s  |  COUNT: 0  |  Format: %s  |  Zoom: %d%%",
                        smin.to_a1().c_str(), smax.to_a1().c_str(), fmt_name, zoom_pct);
        }
    } else {
        cached_has_range_ = false;
        int zoom_pct = static_cast<int>(zoom_ * 100.0f + 0.5f);
        ImGui::Text("Cell: %s  |  Format: %s  |  Sheets: %d  |  Rows: %d  |  Zoom: %d%%",
                    grid_state_.selection.selected_cell.to_a1().c_str(),
                    fmt_name,
                    state.workbook.sheet_count(),
                    sheet.row_count(),
                    zoom_pct);
    }
}

void MainWindow::render_modals(AppState& state) {
    // Unsaved changes modal
    if (show_dirty_new_modal_ || wants_close_)
        ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Discard them?");
        ImGui::Spacing();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            if (wants_close_) {
                should_quit_ = true;
                wants_close_ = false;
            } else {
                state.reset_to_new();
            }
            show_dirty_new_modal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            wants_close_ = false;
            show_dirty_new_modal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Paste Special modal (Ctrl+Shift+V)
    if (show_paste_special_)
        ImGui::OpenPopup("Paste Special");
    if (ImGui::BeginPopupModal("Paste Special", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Choose paste mode:");
        ImGui::Spacing();
        if (ImGui::Button("Values Only", ImVec2(140, 0))) {
            action_paste_special(state, PasteMode::ValuesOnly);
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Formulas Only", ImVec2(140, 0))) {
            action_paste_special(state, PasteMode::FormulasOnly);
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Transpose", ImVec2(140, 0))) {
            action_paste_special(state, PasteMode::Transpose);
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Plugin trust confirmation modal
    if (!state.pending_plugin_path.empty()) {
        if (state.pending_plugin_path != trust_modal_path_) {
            trust_modal_path_ = state.pending_plugin_path;
            trust_modal_hash_ = sha256_of_file(state.pending_plugin_path);
        }
        ImGui::OpenPopup("Trust Plugin?");
    }
    if (ImGui::BeginPopupModal("Trust Plugin?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("An untrusted plugin was dropped:");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", state.pending_plugin_path.c_str());
        ImGui::Spacing();
        ImGui::Text("SHA-256:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", trust_modal_hash_.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Trust & Load", ImVec2(140, 0))) {
            state.plugin_manager.trust_and_load(state.pending_plugin_path);
            state.toasts.show("Plugin loaded");
            state.pending_plugin_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            state.pending_plugin_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Recovery modal — shown on startup if a recovery file exists
    if (state.show_recovery_modal)
        ImGui::OpenPopup("Recover Unsaved Work?");
    if (ImGui::BeginPopupModal("Recover Unsaved Work?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A recovery file was found from a previous session.");
        ImGui::Text("Would you like to restore it?");
        ImGui::Spacing();
        if (ImGui::Button("Restore", ImVec2(120, 0))) {
            auto path = AutoSave::recovery_path();
            if (state.open_file(path.string())) {
                state.current_file.clear();  // don't associate with recovery file
                state.toasts.show("Recovered unsaved work");
            } else {
                state.toasts.show("Recovery failed");
            }
            AutoSave::discard_recovery();
            state.show_recovery_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            AutoSave::discard_recovery();
            state.show_recovery_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

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

void MainWindow::do_open(AppState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CellWright", "magic"}, {"All Files", "*"}};
    if (NFD::OpenDialog(path, filters, 2) == NFD_OKAY) {
        if (!state.open_file(path.get()))
            state.toasts.show("Failed to open file");
    }
}

void MainWindow::do_save(AppState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CellWright", "magic"}};
    std::string default_name;
    if (!state.current_file.empty())
        default_name = std::filesystem::path(state.current_file).filename().string();
    if (NFD::SaveDialog(path, filters, 1, nullptr,
                        default_name.empty() ? nullptr : default_name.c_str()) == NFD_OKAY) {
        if (state.save_file(path.get()))
            state.toasts.show("Saved");
    }
}

void MainWindow::do_import_csv(AppState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CSV", "csv"}, {"All Files", "*"}};
    if (NFD::OpenDialog(path, filters, 2) == NFD_OKAY) {
        if (state.import_csv(path.get()))
            state.toasts.show("CSV imported");
        else
            state.toasts.show("Failed to import CSV");
    }
}

void MainWindow::do_export_csv(AppState& state) {
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{"CSV", "csv"}};
    if (NFD::SaveDialog(path, filters, 1) == NFD_OKAY) {
        if (state.export_csv(path.get()))
            state.toasts.show("CSV exported");
        else
            state.toasts.show("Failed to export CSV");
    }
}

void MainWindow::render_cond_format_editor(AppState& state) {
    auto& store = state.active_state().cond_format;
    ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Conditional Formatting", &show_cond_format_)) {
        ImGui::End();
        return;
    }

    static const char* op_names[] = {">", "<", ">=", "<=", "==", "!="};

    // Existing rules
    int to_remove = -1;
    for (std::size_t i = 0; i < store.size(); ++i) {
        auto& rule = store.rules()[i];
        ImGui::PushID(static_cast<int>(i));

        // Column selector
        ImGui::SetNextItemWidth(60);
        char col_label[8];
        if (rule.column == -1)
            std::snprintf(col_label, sizeof(col_label), "All");
        else
            std::snprintf(col_label, sizeof(col_label), "%s",
                          CellAddress::col_to_letters(rule.column).c_str());
        if (ImGui::BeginCombo("##col", col_label)) {
            if (ImGui::Selectable("All", rule.column == -1))
                rule.column = -1;
            for (int32_t c = 0; c < 26; ++c) {
                auto lbl = CellAddress::col_to_letters(c);
                if (ImGui::Selectable(lbl.c_str(), rule.column == c))
                    rule.column = c;
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        int op_idx = static_cast<int>(rule.op);
        if (ImGui::Combo("##op", &op_idx, op_names, 6))
            rule.op = static_cast<ConditionOp>(op_idx);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputDouble("##val", &rule.threshold, 0, 0, "%.2f");

        ImGui::SameLine();
        float color[4] = {rule.color.r / 255.0f, rule.color.g / 255.0f,
                           rule.color.b / 255.0f, rule.color.a / 255.0f};
        ImGui::SetNextItemWidth(40);
        if (ImGui::ColorEdit4("##clr", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            rule.color = {static_cast<uint8_t>(color[0] * 255),
                          static_cast<uint8_t>(color[1] * 255),
                          static_cast<uint8_t>(color[2] * 255),
                          static_cast<uint8_t>(color[3] * 255)};
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
            to_remove = static_cast<int>(i);

        ImGui::PopID();
    }

    if (to_remove >= 0)
        store.remove_rule(static_cast<std::size_t>(to_remove));

    ImGui::Separator();
    if (ImGui::Button("Add Rule")) {
        ConditionalRule rule;
        rule.column = grid_state_.selection.selected_cell.col;
        store.add_rule(rule);
    }

    ImGui::End();
}

}  // namespace magic
