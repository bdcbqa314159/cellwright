// Part of MainWindow, split out of MainWindow.cpp purely to keep each translation
// unit readable (issue #17). Same class, same behaviour, and no declaration in
// MainWindow.hpp changed. Menu bar rendering.

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

void MainWindow::render_menu_bar(SessionState& state) {
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
                                toasts_.show("Failed to open file");
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
                apply_format({.type = FormatType::GENERAL});
            }
            if (ImGui::MenuItem("Number (2 dp)", nullptr, current.type == FormatType::NUMBER)) {
                apply_format({.type = FormatType::NUMBER, .decimals = 2});
            }
            if (ImGui::MenuItem("Percentage", nullptr, current.type == FormatType::PERCENTAGE)) {
                apply_format({.type = FormatType::PERCENTAGE, .decimals = 1});
            }
            if (ImGui::MenuItem("Currency ($)", nullptr, current.type == FormatType::CURRENCY)) {
                apply_format({.type = FormatType::CURRENCY, .decimals = 2, .currency_symbol = "$"});
            }
            if (ImGui::MenuItem("Scientific", nullptr, current.type == FormatType::SCIENTIFIC)) {
                apply_format({.type = FormatType::SCIENTIFIC, .decimals = 2});
            }
            if (ImGui::MenuItem("Date (ISO)", nullptr, current.type == FormatType::DATE)) {
                apply_format({.type = FormatType::DATE});
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
                            font_rebuild_needed_ = true;
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

}  // namespace magic
