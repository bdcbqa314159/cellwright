#include "ui/MainWindow.hpp"
#include "app/AppState.hpp"
#include "core/Workbook.hpp"
#include "core/Clipboard.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "util/Sha256.hpp"
#include <imgui.h>
#include <cstring>
#include <algorithm>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

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
                file_dialog_.show_open = true;
                snprintf(file_dialog_.path_buf, sizeof(file_dialog_.path_buf), "%s", state.current_file.c_str());
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!state.current_file.empty()) {
                    WorkbookIO::save(state.current_file, state.workbook);
                    state.mark_saved();
                    state.toasts.show("Saved");
                } else {
                    file_dialog_.show_save = true;
                }
            }
            if (ImGui::MenuItem("Save As...")) {
                file_dialog_.show_save = true;
                snprintf(file_dialog_.path_buf, sizeof(file_dialog_.path_buf), "%s", state.current_file.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import CSV...")) {
                file_dialog_.show_import_csv = true;
                file_dialog_.path_buf[0] = '\0';
            }
            if (ImGui::MenuItem("Export CSV...")) {
                file_dialog_.show_export_csv = true;
                file_dialog_.path_buf[0] = '\0';
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, as.undo_manager.can_undo())) {
                auto& sheet = state.workbook.active_sheet();
                as.undo_manager.undo(sheet);
                if (auto cell = as.undo_manager.last_affected())
                    ci.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, as.undo_manager.can_redo())) {
                auto& sheet = state.workbook.active_sheet();
                as.undo_manager.redo(sheet);
                if (auto cell = as.undo_manager.last_affected())
                    ci.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                ci.copy(state.clipboard, state.workbook.active_sheet(),
                        grid_state_.selected, grid_state_.has_range_selection,
                        grid_state_.sel_min(), grid_state_.sel_max());
            }
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                ci.cut(state.clipboard, state.workbook.active_sheet(),
                       grid_state_.selected, grid_state_.has_range_selection,
                       grid_state_.sel_min(), grid_state_.sel_max());
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, state.clipboard.has_data())) {
                ci.paste(state.clipboard, state.workbook.active_sheet(), grid_state_.selected,
                         as.undo_manager, as.format_map, as.dep_graph, state.workbook);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Format")) {
            auto& addr = grid_state_.selected;
            CellFormat current = as.format_map.get(addr);

            auto apply_format = [&](CellFormat fmt) {
                if (grid_state_.has_range_selection) {
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
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // ── File dialogs (simple InputText popups) ──────────────────────────────
    auto file_popup = [&](const char* id, bool& show, auto callback) {
        if (show) ImGui::OpenPopup(id);
        if (ImGui::BeginPopupModal(id, &show, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Path", file_dialog_.path_buf, sizeof(file_dialog_.path_buf));
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                callback(std::string(file_dialog_.path_buf));
                show = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                show = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    };

    file_popup("Open File", file_dialog_.show_open, [&](const std::string& path) {
        state.open_file(path);
    });

    file_popup("Save File", file_dialog_.show_save, [&](const std::string& path) {
        if (WorkbookIO::save(path, state.workbook)) {
            state.current_file = path;
            state.mark_saved();
            state.toasts.show("Saved");
        }
    });

    file_popup("Import CSV", file_dialog_.show_import_csv, [&](const std::string& path) {
        auto& sheet = state.workbook.active_sheet();
        CsvIO::import_file(path, sheet);
        as.dep_graph.clear();
        as.format_map = FormatMap{};
        as.undo_manager.clear();
        state.toasts.show("CSV imported");
    });

    file_popup("Export CSV", file_dialog_.show_export_csv, [&](const std::string& path) {
        CsvIO::export_file(path, state.workbook.active_sheet());
    });
}

// ── Keyboard shortcuts ──────────────────────────────────────────────────────

void MainWindow::handle_keyboard(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    auto& ci = state.cell_input;
    auto& as = state.active_state();

    // Don't handle shortcuts when typing in an input field
    if (ImGui::GetIO().WantTextInput) return;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        auto& sheet = state.workbook.active_sheet();
        as.undo_manager.undo(sheet);
        if (auto cell = as.undo_manager.last_affected())
            ci.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        auto& sheet = state.workbook.active_sheet();
        as.undo_manager.redo(sheet);
        if (auto cell = as.undo_manager.last_affected())
            ci.recalc_dependents(sheet, *cell, as.dep_graph, &state.workbook);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        ci.copy(state.clipboard, state.workbook.active_sheet(),
                grid_state_.selected, grid_state_.has_range_selection,
                grid_state_.sel_min(), grid_state_.sel_max());
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        ci.cut(state.clipboard, state.workbook.active_sheet(),
               grid_state_.selected, grid_state_.has_range_selection,
               grid_state_.sel_min(), grid_state_.sel_max());
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V) && state.clipboard.has_data()) {
        ci.paste(state.clipboard, state.workbook.active_sheet(), grid_state_.selected,
                 as.undo_manager, as.format_map, as.dep_graph, state.workbook);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (!state.current_file.empty()) {
            WorkbookIO::save(state.current_file, state.workbook);
            state.mark_saved();
            state.toasts.show("Saved");
        } else {
            file_dialog_.show_save = true;
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        file_dialog_.show_open = true;
    }

    // When not editing a cell
    if (!grid_state_.editor.is_editing()) {
        bool shift = io.KeyShift;

        // Arrow key navigation (Shift extends selection)
        auto move = [&](int dc, int dr) {
            if (!shift) {
                grid_state_.has_range_selection = false;
                grid_state_.sel_anchor = grid_state_.selected;
            } else if (!grid_state_.has_range_selection) {
                grid_state_.has_range_selection = true;
                grid_state_.sel_anchor = grid_state_.selected;
            }
            grid_state_.selected.col = std::max(0, grid_state_.selected.col + dc);
            grid_state_.selected.row = std::max(0, grid_state_.selected.row + dr);
        };

        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    move(0, -1);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))   move(0, 1);
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))   move(-1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))  move(1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            grid_state_.has_range_selection = false;
            grid_state_.selected.col++;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            grid_state_.has_range_selection = false;
            grid_state_.selected.row++;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            auto& sheet = state.workbook.active_sheet();
            if (grid_state_.has_range_selection) {
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
                ci.process("", sheet, grid_state_.selected,
                           as.undo_manager, as.format_map, as.dep_graph, state.workbook);
            }
        }

        // F2 to edit current cell
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
            auto& sheet = state.workbook.active_sheet();
            std::string initial;
            if (sheet.has_formula(grid_state_.selected))
                initial = "=" + sheet.get_formula(grid_state_.selected);
            else
                initial = to_display_string(sheet.get_value(grid_state_.selected));
            grid_state_.editor.begin_edit(grid_state_.selected, initial);
        }

        // Start typing on selected cell -> begin editing
        // Seed buffer with first char (InputText won't process queue on activation frame)
        if (!ctrl && !io.KeyAlt) {
            ImGuiIO& input_io = ImGui::GetIO();
            if (input_io.InputQueueCharacters.Size > 0) {
                ImWchar wch = input_io.InputQueueCharacters[0];
                if (wch >= 32 && wch < 127) {
                    std::string seed(1, static_cast<char>(wch));
                    input_io.InputQueueCharacters.erase(input_io.InputQueueCharacters.Data);
                    grid_state_.editor.begin_edit(grid_state_.selected, seed, false);
                }
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

    ImGui::Begin("Magic Dashboard", nullptr, window_flags);

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
    if (formula_bar_.render(sheet, grid_state_.selected, cell_editing)) {
        ci.process(formula_bar_.buffer(), sheet, grid_state_.selected,
                   as.undo_manager, as.format_map, as.dep_graph, state.workbook);
    }

    ImGui::Separator();

    // Pass formula bar buffer to grid for reference highlighting
    grid_state_.formula_bar_buf = formula_bar_.is_formula_mode()
        ? formula_bar_.buffer() : nullptr;
    grid_state_.dark_theme = (theme_ == Theme::Dark);

    // Spreadsheet grid
    if (grid_.render(sheet, grid_state_, as.format_map)) {
        ci.process(grid_state_.editor.buffer(), sheet,
                   grid_state_.editor.editing_cell(),
                   as.undo_manager, as.format_map, as.dep_graph, state.workbook);
    }

    // Handle cell drag completions (move / fill)
    if (grid_state_.drag_completed) {
        grid_state_.drag_completed = false;
        CellAddress src = grid_state_.drag_source;
        CellAddress tgt = grid_state_.drag_target;

        if (grid_state_.drag_mode == CellDragMode::Move && !(src == tgt)) {
            // Move: transfer source cell to target, clear source
            CellValue src_val = sheet.get_value(src);
            std::string src_formula = sheet.get_formula(src);

            // Set target cell
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
            // Clear source cell
            ci.process("", sheet, src,
                       as.undo_manager, as.format_map, as.dep_graph, state.workbook);
            grid_state_.selected = tgt;
            grid_state_.sel_anchor = tgt;
            grid_state_.has_range_selection = false;
        } else if (grid_state_.drag_mode == CellDragMode::Fill && !(src == tgt)) {
            // Fill: 2D rectangular fill from source to target (batch recalc)
            CellValue src_val = sheet.get_value(src);
            std::string src_formula = sheet.get_formula(src);

            int32_t c1 = std::min(src.col, tgt.col);
            int32_t c2 = std::max(src.col, tgt.col);
            int32_t r1 = std::min(src.row, tgt.row);
            int32_t r2 = std::max(src.row, tgt.row);

            // Phase 1: set all cells without recalc
            std::unordered_set<CellAddress> changed_cells;
            for (int32_t c = c1; c <= c2; ++c) {
                for (int32_t r = r1; r <= r2; ++r) {
                    if (c == src.col && r == src.row) continue;
                    CellAddress fill_addr{c, r};
                    if (!src_formula.empty()) {
                        std::string adjusted = Clipboard::adjust_references(
                            src_formula, c - src.col, r - src.row);
                        ci.process_no_recalc(("=" + adjusted).c_str(), sheet, fill_addr,
                                             as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                    } else {
                        std::string display = to_display_string(src_val);
                        ci.process_no_recalc(display.c_str(), sheet, fill_addr,
                                             as.undo_manager, as.format_map, as.dep_graph, state.workbook);
                    }
                    changed_cells.insert(fill_addr);
                }
            }

            // Phase 2: one batch recalc for all changed cells
            ci.batch_recalc(sheet, changed_cells, as.dep_graph, &state.workbook);
        }
        grid_state_.drag_mode = CellDragMode::None;
    }

    // Sheet tabs
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
        // "+" tab to add sheet
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            state.workbook.add_sheet();
            state.ensure_sheet_states();
        }
        ImGui::EndTabBar();
    }

    // Status bar
    ImGui::Separator();
    auto fmt = as.format_map.get(grid_state_.selected);
    const char* fmt_name = "General";
    switch (fmt.type) {
        case FormatType::NUMBER: fmt_name = "Number"; break;
        case FormatType::PERCENTAGE: fmt_name = "%"; break;
        case FormatType::CURRENCY: fmt_name = "$"; break;
        case FormatType::SCIENTIFIC: fmt_name = "Sci"; break;
        case FormatType::DATE: fmt_name = "Date"; break;
        default: break;
    }

    // Selection stats (SUM, AVG, COUNT) when range selected (cached)
    if (grid_state_.has_range_selection) {
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
                    grid_state_.selected.to_a1().c_str(),
                    fmt_name,
                    state.workbook.sheet_count(),
                    sheet.row_count(),
                    zoom_pct);
    }

    ImGui::End();

    // Render toast notifications
    state.toasts.render();

    // ── Unsaved changes modal ────────────────────────────────────────────────
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

    // ── Plugin trust confirmation modal ─────────────────────────────────────
    if (!state.pending_plugin_path.empty()) {
        // Cache hash on first frame modal opens
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

    // Render chart panel as a separate dockable window
    chart_panel_.render(sheet);

    // Render SQL query panel
    sql_panel_.render(state.duckdb_engine, sheet);

    // Render plugin panels as dockable windows
    state.plugin_manager.render_panels();
}

}  // namespace magic
