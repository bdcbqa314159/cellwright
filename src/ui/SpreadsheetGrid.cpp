#include "ui/SpreadsheetGrid.hpp"
#include "ui/FormulaBar.hpp"
#include "core/Sheet.hpp"
#include "core/CellFormat.hpp"
#include "core/ConditionalFormat.hpp"
#include "core/RowFilter.hpp"
#include "formula/Tokenizer.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace magic {

// Palette for formula reference highlighting (semi-transparent)
static constexpr int NUM_REF_COLORS = 6;

// Dark theme: higher alpha needed on dark backgrounds
static const ImU32 ref_palette_dark[NUM_REF_COLORS] = {
    IM_COL32(66,  133, 244, 70),  // blue
    IM_COL32(52,  168,  83, 70),  // green
    IM_COL32(154,  71, 237, 70),  // purple
    IM_COL32(234, 134,   0, 70),  // orange
    IM_COL32(234,  67,  53, 70),  // red
    IM_COL32(0,   172, 193, 70),  // teal
};

// Light theme: lower alpha since colors are vivid on white
static const ImU32 ref_palette_light[NUM_REF_COLORS] = {
    IM_COL32(66,  133, 244, 50),
    IM_COL32(52,  168,  83, 50),
    IM_COL32(154,  71, 237, 50),
    IM_COL32(234, 134,   0, 50),
    IM_COL32(234,  67,  53, 50),
    IM_COL32(0,   172, 193, 50),
};

// Parse the formula buffer and map each referenced cell to a color index.
static std::unordered_map<CellAddress, int> build_ref_colors(const char* buf) {
    std::unordered_map<CellAddress, int> colors;
    if (!buf || buf[0] != '=') return colors;

    try {
        auto tokens = Tokenizer::tokenize(std::string(buf + 1));
        int color_idx = 0;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i].type != TokenType::CELLREF) continue;

            auto from = CellAddress::from_a1(tokens[i].text);
            if (!from) continue;

            // Check for range pattern: CELLREF COLON CELLREF
            if (i + 2 < tokens.size() &&
                tokens[i + 1].type == TokenType::COLON &&
                tokens[i + 2].type == TokenType::CELLREF) {
                auto to = CellAddress::from_a1(tokens[i + 2].text);
                if (to) {
                    int32_t c1 = std::min(from->col, to->col), c2 = std::max(from->col, to->col);
                    int32_t r1 = std::min(from->row, to->row), r2 = std::max(from->row, to->row);
                    for (int32_t c = c1; c <= c2; ++c)
                        for (int32_t r = r1; r <= r2; ++r)
                            colors[{c, r}] = color_idx;
                }
                i += 2; // skip colon + second cellref
            } else {
                colors[*from] = color_idx;
            }
            color_idx = (color_idx + 1) % NUM_REF_COLORS;
        }
    } catch (...) {
        // Incomplete formula while typing — silently ignore
    }
    return colors;
}

static CellZone detect_zone(const ImVec2& mouse, const ImVec2& rmin, const ImVec2& rmax) {
    constexpr float FILL_SIZE = 5.0f;
    constexpr float EDGE_THRESH = 3.0f;

    // Outside the rect entirely
    if (mouse.x < rmin.x || mouse.x > rmax.x || mouse.y < rmin.y || mouse.y > rmax.y)
        return CellZone::None;

    // Fill handle: bottom-right corner square
    if (mouse.x >= rmax.x - FILL_SIZE && mouse.y >= rmax.y - FILL_SIZE)
        return CellZone::FillHandle;

    // Edge: within EDGE_THRESH of any border
    if (mouse.x - rmin.x < EDGE_THRESH || rmax.x - mouse.x < EDGE_THRESH ||
        mouse.y - rmin.y < EDGE_THRESH || rmax.y - mouse.y < EDGE_THRESH)
        return CellZone::Edge;

    return CellZone::Interior;
}

bool SpreadsheetGrid::render(Sheet& sheet, GridState& state, const FormatMap& formats) {
    bool committed = false;
    int num_cols = std::min(std::max(sheet.col_count(), 26), 256);
    int num_rows = sheet.row_count();

    if (num_rows < 100) num_rows = 100;

    // Zone detection and cursor setting (uses rect captured from previous frame)
    bool in_formula_or_edit = state.editor.is_formula_mode() || state.editor.is_editing();
    CellZone hovered_zone = CellZone::None;
    if (!in_formula_or_edit && state.drag.drag_mode == CellDragMode::None) {
        ImVec2 mouse = ImGui::GetMousePos();
        hovered_zone = detect_zone(mouse, state.selected_rect_min, state.selected_rect_max);
        switch (hovered_zone) {
            case CellZone::FillHandle: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); break;
            case CellZone::Edge:       ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); break;
            default: break;
        }
    }

    // Drag initiation on mouse press
    if (!in_formula_or_edit && state.drag.drag_mode == CellDragMode::None &&
        ImGui::IsMouseClicked(0) && hovered_zone != CellZone::None) {
        switch (hovered_zone) {
            case CellZone::Edge:
                state.drag.drag_mode = CellDragMode::Move;
                state.drag.drag_source = state.selection.selected_cell;
                break;
            case CellZone::Interior:
                state.drag.drag_mode = CellDragMode::Select;
                state.selection.sel_anchor = state.selection.selected_cell;
                state.selection.has_range = false;
                break;
            case CellZone::FillHandle:
                state.drag.drag_mode = CellDragMode::Fill;
                state.drag.drag_source = state.selection.selected_cell;
                break;
            default: break;
        }
    }

    int table_cols = num_cols + 1;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable;

    if (!ImGui::BeginTable("spreadsheet", table_cols, flags)) return false;

    // Freeze header row + row-number column, plus user-configured extra panes
    ImGui::TableSetupScrollFreeze(1 + state.freeze_cols, 1 + state.freeze_rows);

    // Ensure col_widths_ vector is big enough
    if (static_cast<int>(col_widths_.size()) < num_cols)
        col_widths_.resize(num_cols, DEFAULT_COL_WIDTH);

    ImGui::TableSetupColumn("##rownum", ImGuiTableColumnFlags_WidthFixed, ROW_NUM_WIDTH);
    for (int c = 0; c < num_cols; ++c) {
        ImGui::TableSetupColumn(CellAddress::col_to_letters(c).c_str(),
                                ImGuiTableColumnFlags_WidthFixed, col_widths_[c]);
    }
    // Manual header row with right-click sort menu
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ImGui::TableSetColumnIndex(0);
    ImGui::TableHeader("##rownum");
    for (int c = 0; c < num_cols; ++c) {
        ImGui::TableSetColumnIndex(c + 1);
        ImGui::TableHeader(CellAddress::col_to_letters(c).c_str());
        ImGui::PushID(c + 10000);
        if (ImGui::BeginPopupContextItem("##colhdr")) {
            if (ImGui::MenuItem("Sort A \xe2\x86\x92 Z")) {
                state.context_col = c;
                state.context_action = ContextAction::SortAsc;
            }
            if (ImGui::MenuItem("Sort Z \xe2\x86\x92 A")) {
                state.context_col = c;
                state.context_action = ContextAction::SortDesc;
            }
            if (state.row_filter) {
                ImGui::Separator();
                // Inline filter input
                char filter_buf[64] = {};
                auto existing = state.row_filter->get_filter(c);
                std::strncpy(filter_buf, existing.c_str(), sizeof(filter_buf) - 1);
                ImGui::SetNextItemWidth(120);
                if (ImGui::InputText("Filter", filter_buf, sizeof(filter_buf),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
                    state.row_filter->set_filter(c, filter_buf);
                    ImGui::CloseCurrentPopup();
                }
                if (!existing.empty() && ImGui::MenuItem("Clear Filter")) {
                    state.row_filter->clear_filter(c);
                }
                if (state.row_filter->has_filters() && ImGui::MenuItem("Clear All Filters")) {
                    state.row_filter->clear_all();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // Filter row — render small InputText per column for auto-filter
    if (state.row_filter && state.row_filter->has_filters()) {
        // Show a filter indicator row
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("\xf0\x9f\x94\x8d");  // magnifying glass emoji
        for (int c = 0; c < num_cols; ++c) {
            ImGui::TableSetColumnIndex(c + 1);
            auto filter_text = state.row_filter->get_filter(c);
            if (!filter_text.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
                ImGui::TextUnformatted(filter_text.c_str());
                ImGui::PopStyleColor();
            }
        }
    }

    // Compute visible rows (respects active filters).
    // When no filters are active, skip the indirection entirely.
    bool filtering = state.row_filter && state.row_filter->has_filters();
    const std::vector<int32_t>* visible_rows_ptr = nullptr;
    int visible_count = num_rows;
    if (filtering) {
        visible_rows_ptr = &state.row_filter->update(sheet, sheet.value_generation());
        visible_count = static_cast<int>(visible_rows_ptr->size());
    }

    // Track which cell the mouse hovers this frame (for formula drag-end detection)
    CellAddress drag_hover{-1, -1};

    // Cancel drag if we left formula mode (e.g. Escape)
    if (state.drag.formula_dragging && !state.editor.is_formula_mode())
        state.drag.formula_dragging = false;

    // Build reference highlight map for formula mode (cached)
    // Use cell editor buffer if editing in-cell, otherwise formula bar buffer
    const char* ref_buf = state.editor.is_formula_mode() ? state.editor.buffer()
                        : state.formula_bar_buf;
    std::string ref_buf_str = ref_buf ? ref_buf : "";
    if (ref_buf_str != state.cached_ref_buffer) {
        state.cached_ref_buffer = ref_buf_str;
        state.cached_ref_colors = build_ref_colors(ref_buf);
    }
    const auto& ref_colors = state.cached_ref_colors;
    const ImU32* ref_palette = state.dark_theme ? ref_palette_dark : ref_palette_light;
    const ImU32 formula_drag_color = state.dark_theme ? IM_COL32(66, 133, 244, 100) : IM_COL32(66, 133, 244, 60);
    const ImU32 fill_drag_color    = state.dark_theme ? IM_COL32(52, 168, 83, 80)   : IM_COL32(52, 168, 83, 50);
    const ImVec4 error_text_color  = state.dark_theme ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.85f, 0.15f, 0.15f, 1.0f);
    const ImU32 fill_handle_color  = state.dark_theme ? IM_COL32(0, 120, 215, 255)  : IM_COL32(0, 90, 180, 255);

    // Precompute selection bounds once (avoid per-cell min/max)
    auto sel_min = state.sel_min();
    auto sel_max = state.sel_max();

    // Precompute drag highlight bounds once (avoid per-cell min/max)
    bool has_formula_drag_highlight = state.drag.formula_dragging && state.drag.formula_drag_target.col >= 0;
    int fd_c1 = 0, fd_c2 = -1, fd_r1 = 0, fd_r2 = -1;
    if (has_formula_drag_highlight) {
        fd_c1 = std::min(state.drag.formula_drag_origin.col, state.drag.formula_drag_target.col);
        fd_c2 = std::max(state.drag.formula_drag_origin.col, state.drag.formula_drag_target.col);
        fd_r1 = std::min(state.drag.formula_drag_origin.row, state.drag.formula_drag_target.row);
        fd_r2 = std::max(state.drag.formula_drag_origin.row, state.drag.formula_drag_target.row);
    }

    // Move drag: highlight target cell and show floating ghost
    bool has_move_drag_highlight = state.drag.drag_mode == CellDragMode::Move && state.drag.drag_target.col >= 0;
    const ImU32 move_drag_color = state.dark_theme ? IM_COL32(100, 149, 237, 100) : IM_COL32(100, 149, 237, 70);
    if (has_move_drag_highlight) {
        CellValue drag_val = sheet.get_value(state.drag.drag_source);
        std::string drag_text = to_display_string(drag_val);
        if (!drag_text.empty()) {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetMousePos().x + 14, ImGui::GetMousePos().y + 14));
            ImGuiWindowFlags ghost_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoInputs;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 3));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.3f, 0.5f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
            if (ImGui::Begin("##drag_ghost", nullptr, ghost_flags)) {
                ImGui::TextUnformatted(drag_text.c_str());
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }
    }
    bool has_fill_drag_highlight = state.drag.drag_mode == CellDragMode::Fill && state.drag.drag_target.col >= 0;
    int fl_c1 = 0, fl_c2 = -1, fl_r1 = 0, fl_r2 = -1;
    if (has_fill_drag_highlight) {
        fl_c1 = std::min(state.drag.drag_source.col, state.drag.drag_target.col);
        fl_c2 = std::max(state.drag.drag_source.col, state.drag.drag_target.col);
        fl_r1 = std::min(state.drag.drag_source.row, state.drag.drag_target.row);
        fl_r2 = std::max(state.drag.drag_source.row, state.drag.drag_target.row);
    }

    // Marching ants: track screen rect of clipboard range
    ImVec2 ants_min{1e9f, 1e9f}, ants_max{-1e9f, -1e9f};
    bool ants_visible = false;

    ImGuiListClipper clipper;
    clipper.Begin(visible_count);

    std::string display;
    while (clipper.Step()) {
        for (int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi) {
            int row = filtering ? (*visible_rows_ptr)[vi] : vi;
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%d", row + 1);

            for (int col = 0; col < num_cols; ++col) {
                ImGui::TableSetColumnIndex(col + 1);

                CellAddress addr{col, row};
                ImGui::PushID(row * num_cols + col);

                // Highlight find/search matches
                if (state.find_matches) {
                    for (int fi = 0; fi < static_cast<int>(state.find_matches->size()); ++fi) {
                        if ((*state.find_matches)[fi] == addr) {
                            ImU32 match_color = (fi == state.find_match_index)
                                ? IM_COL32(255, 200, 0, 100)   // current match
                                : IM_COL32(255, 255, 0, 50);   // other matches
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, match_color);
                            break;
                        }
                    }
                }

                // Highlight cells referenced by the formula being edited
                auto ref_it = ref_colors.find(addr);
                if (ref_it != ref_colors.end())
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                           ref_palette[ref_it->second]);

                // Live highlight of the range being dragged in formula mode
                if (has_formula_drag_highlight &&
                    col >= fd_c1 && col <= fd_c2 && row >= fd_r1 && row <= fd_r2)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                           formula_drag_color);

                // Move drag: highlight target cell
                if (has_move_drag_highlight &&
                    col == state.drag.drag_target.col && row == state.drag.drag_target.row)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, move_drag_color);
                // Live highlight of the fill-drag target range
                if (has_fill_drag_highlight &&
                    col >= fl_c1 && col <= fl_c2 && row >= fl_r1 && row <= fl_r2)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                           fill_drag_color);

                bool is_selected = (state.selection.selected_cell == addr);
                // Highlight cells in selection range
                if (!is_selected && state.selection.has_range) {
                    is_selected = (col >= sel_min.col && col <= sel_max.col &&
                                   row >= sel_min.row && row <= sel_max.row);
                }
                bool is_editing = state.editor.is_editing() && (state.editor.editing_cell() == addr);

                if (is_editing) {
                    ImGui::SetNextItemWidth(-1);
                    ImVec2 edit_min = ImGui::GetCursorScreenPos();
                    if (state.editor.render(state.registry, state.mono_font)) {
                        committed = true;
                    }
                    // Green border for in-cell editing (distinct from blue selection)
                    ImVec2 edit_max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(
                        edit_min, edit_max, IM_COL32(0, 180, 0, 255), 0.0f, 0, 2.0f);
                } else {
                    CellValue val = sheet.get_value(addr);
                    CellFormat fmt = formats.get(addr);
                    display = format_value(val, fmt);

                    // Capture cell top-left before any cursor adjustments
                    ImVec2 cell_min = ImGui::GetCursorScreenPos();

                    // Truncate text with ellipsis if it overflows cell width
                    float avail = ImGui::GetContentRegionAvail().x;
                    float text_w = ImGui::CalcTextSize(display.c_str()).x;
                    bool truncated = false;
                    if (text_w > avail && avail > 0 && !display.empty()) {
                        truncated = true;
                        float ellipsis_w = ImGui::CalcTextSize("...").x;
                        if (avail > ellipsis_w) {
                            // Binary search for how many chars fit
                            size_t lo = 0, hi = display.size();
                            while (lo < hi) {
                                size_t mid = (lo + hi + 1) / 2;
                                if (ImGui::CalcTextSize(display.c_str(), display.c_str() + mid).x + ellipsis_w <= avail)
                                    lo = mid;
                                else
                                    hi = mid - 1;
                            }
                            if (lo > 0)
                                display = display.substr(0, lo) + "...";
                        }
                        text_w = ImGui::CalcTextSize(display.c_str()).x;
                    }

                    // Right-align numbers
                    if (is_number(val)) {
                        if (text_w < avail) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - text_w);
                    }

                    // Color errors red (text + light background tint)
                    if (is_error(val)) {
                        ImGui::PushStyleColor(ImGuiCol_Text, error_text_color);
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(255, 0, 0, 25));
                    }

                    // Conditional formatting — apply background color from first matching rule
                    if (state.cond_format && is_number(val)) {
                        if (auto* cc = state.cond_format->evaluate(col, as_number(val))) {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                                   IM_COL32(cc->r, cc->g, cc->b, cc->a));
                        }
                    }

                    if (ImGui::Selectable(display.empty() ? "##empty" : display.c_str(),
                                          is_selected,
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (!state.editor.is_formula_mode() &&
                            state.drag.drag_mode == CellDragMode::None) {
                            state.selection.selected_cell = addr;
                            state.selection.sel_anchor = addr;
                            state.selection.has_range = false;

                            // Double click only to enter edit mode (#33)
                            if (ImGui::IsMouseDoubleClicked(0)) {
                                std::string initial;
                                if (sheet.has_formula(addr))
                                    initial = "=" + sheet.get_formula(addr);
                                else
                                    initial = to_display_string(sheet.get_value(addr));
                                state.editor.begin_edit(addr, initial);
                            }
                        }
                    }

                    // Right-click context menu
                    if (ImGui::BeginPopupContextItem("##cellctx")) {
                        if (ImGui::MenuItem("Cut"))   state.context_action = ContextAction::Cut;
                        if (ImGui::MenuItem("Copy"))  state.context_action = ContextAction::Copy;
                        if (ImGui::MenuItem("Paste")) state.context_action = ContextAction::Paste;
                        if (ImGui::MenuItem("Clear")) state.context_action = ContextAction::Clear;
                        ImGui::Separator();
                        if (ImGui::MenuItem("Insert Row"))    state.context_action = ContextAction::InsertRow;
                        if (ImGui::MenuItem("Insert Column")) state.context_action = ContextAction::InsertCol;
                        if (ImGui::MenuItem("Delete Row"))    state.context_action = ContextAction::DeleteRow;
                        if (ImGui::MenuItem("Delete Column")) state.context_action = ContextAction::DeleteCol;
                        ImGui::EndPopup();
                    }

                    // Tooltip for date cells, truncated cells, and formula cells
                    if (ImGui::IsItemHovered()) {
                        if (fmt.type == FormatType::DATE && !fmt.date_input_hint.empty()) {
                            ImGui::SetTooltip("%s", fmt.date_input_hint.c_str());
                        } else if (sheet.has_formula(addr)) {
                            std::string full = format_value(val, fmt);
                            ImGui::SetTooltip("=%s\n%s", sheet.get_formula(addr).c_str(), full.c_str());
                        } else if (truncated) {
                            // Show full untruncated text
                            std::string full = format_value(val, fmt);
                            ImGui::SetTooltip("%s", full.c_str());
                        }
                    }

                    // Capture selected cell rect using full cell bounds
                    // GetItemRectMax gives correct bottom-right (Selectable spans cell width),
                    // but use cell_min for top-left since right-align may shift cursor.
                    if (state.selection.selected_cell == addr) {
                        state.selected_rect_min = cell_min;
                        state.selected_rect_max = ImGui::GetItemRectMax();

                        if (!in_formula_or_edit) {
                            constexpr float FILL_SIZE = 5.0f;
                            ImVec2 br = state.selected_rect_max;
                            ImVec2 fill_min(br.x - FILL_SIZE, br.y - FILL_SIZE);
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                fill_min, br, fill_handle_color);
                        }

                        // Active cell border (2px blue)
                        ImGui::GetWindowDrawList()->AddRect(
                            state.selected_rect_min, state.selected_rect_max,
                            fill_handle_color, 0.0f, 0, 2.0f);
                    }

                    // Track hovered cell for drag operations.
                    // In formula mode, the active InputText sets
                    // ActiveIdUsingAllKeyboardKeys which blocks
                    // IsItemHovered via IsWindowContentHoverable.
                    // Use raw rect test to bypass that.
                    bool cell_hovered;
                    if (state.editor.is_formula_mode()) {
                        cell_hovered = ImGui::IsMouseHoveringRect(
                            ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                    } else {
                        cell_hovered = ImGui::IsItemHovered(
                            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    }
                    if (cell_hovered) {
                        drag_hover = addr;

                        // Formula-mode drag start (cell editor or formula bar)
                        bool in_formula_mode = state.editor.is_formula_mode() ||
                            (state.formula_bar && state.formula_bar->is_formula_mode());
                        if (in_formula_mode && ImGui::IsMouseClicked(0)) {
                            state.drag.formula_drag_origin = addr;
                            state.drag.formula_dragging = true;
                        }
                    }

                    if (is_error(val)) ImGui::PopStyleColor();

                    // Track clipboard range bounds for marching ants
                    if (state.clipboard_visual.show_marching_ants &&
                        col >= state.clipboard_visual.clip_min.col && col <= state.clipboard_visual.clip_max.col &&
                        row >= state.clipboard_visual.clip_min.row && row <= state.clipboard_visual.clip_max.row) {
                        ImVec2 r_min = cell_min;
                        ImVec2 r_max = ImGui::GetItemRectMax();
                        if (r_min.x < ants_min.x) ants_min.x = r_min.x;
                        if (r_min.y < ants_min.y) ants_min.y = r_min.y;
                        if (r_max.x > ants_max.x) ants_max.x = r_max.x;
                        if (r_max.y > ants_max.y) ants_max.y = r_max.y;
                        ants_visible = true;
                    }
                }

                ImGui::PopID();
            }
        }
    }

    ImGui::EndTable();

    // Draw marching ants around clipboard source range
    if (ants_visible) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float t = static_cast<float>(ImGui::GetTime());
        float dash_len = 6.0f;
        float offset = std::fmod(t * 20.0f, dash_len * 2.0f);

        auto draw_dashed_line = [&](ImVec2 a, ImVec2 b) {
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f) return;
            float nx = dx / len, ny = dy / len;
            float pos = -offset;
            while (pos < len) {
                float seg_start = std::max(pos, 0.0f);
                float seg_end = std::min(pos + dash_len, len);
                if (seg_start < seg_end) {
                    dl->AddLine(
                        ImVec2(a.x + nx * seg_start, a.y + ny * seg_start),
                        ImVec2(a.x + nx * seg_end, a.y + ny * seg_end),
                        IM_COL32(0, 120, 215, 255), 2.0f);
                }
                pos += dash_len * 2.0f;
            }
        };

        draw_dashed_line(ImVec2(ants_min.x, ants_min.y), ImVec2(ants_max.x, ants_min.y)); // top
        draw_dashed_line(ImVec2(ants_max.x, ants_min.y), ImVec2(ants_max.x, ants_max.y)); // right
        draw_dashed_line(ImVec2(ants_max.x, ants_max.y), ImVec2(ants_min.x, ants_max.y)); // bottom
        draw_dashed_line(ImVec2(ants_min.x, ants_max.y), ImVec2(ants_min.x, ants_min.y)); // left
    }

    // Update formula drag target each frame for visual feedback
    if (state.drag.formula_dragging && ImGui::IsMouseDown(0)) {
        if (drag_hover.col >= 0)
            state.drag.formula_drag_target = drag_hover;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    // Formula-mode drag end: mouse released over a cell
    if (state.drag.formula_dragging && ImGui::IsMouseReleased(0)) {
        if (drag_hover.col >= 0) {
            std::string ref = (state.drag.formula_drag_origin == drag_hover)
                ? drag_hover.to_a1()
                : state.drag.formula_drag_origin.to_a1() + ":" + drag_hover.to_a1();
            if (state.editor.is_formula_mode())
                state.editor.insert_ref(ref);
            else if (state.formula_bar && state.formula_bar->is_formula_mode())
                state.formula_bar->insert_ref(ref);
        }
        state.drag.formula_dragging = false;
        state.drag.formula_drag_target = {-1, -1};
    }

    // Cell drag tracking while mouse is held
    if (state.drag.drag_mode != CellDragMode::None && ImGui::IsMouseDown(0)) {
        if (drag_hover.col >= 0 && drag_hover.row >= 0) {
            state.drag.drag_target = drag_hover;
            if (state.drag.drag_mode == CellDragMode::Select) {
                state.selection.selected_cell = drag_hover;
                if (!(drag_hover == state.selection.sel_anchor))
                    state.selection.has_range = true;
            }
        }
        // Set cursor during drag
        switch (state.drag.drag_mode) {
            case CellDragMode::Move: ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); break;
            case CellDragMode::Fill: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); break;
            default: break;
        }
    }

    // Drag completion on mouse release
    if (state.drag.drag_mode != CellDragMode::None && ImGui::IsMouseReleased(0)) {
        if (state.drag.drag_mode == CellDragMode::Move || state.drag.drag_mode == CellDragMode::Fill) {
            if (drag_hover.col >= 0 && drag_hover.row >= 0) {
                state.drag.drag_target = drag_hover;
                state.drag.drag_completed = true;
            }
        }
        // drag_mode is preserved so handle_drag_completion() can inspect it;
        // it resets drag_mode after processing.
        if (!state.drag.drag_completed)
            state.drag.drag_mode = CellDragMode::None;
    }

    return committed;
}

}  // namespace magic
