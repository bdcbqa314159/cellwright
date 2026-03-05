#include "ui/SpreadsheetGrid.hpp"
#include "core/Sheet.hpp"
#include "core/CellFormat.hpp"
#include "formula/Tokenizer.hpp"
#include <imgui.h>
#include <algorithm>

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
                    int c1 = std::min(from->col, to->col), c2 = std::max(from->col, to->col);
                    int r1 = std::min(from->row, to->row), r2 = std::max(from->row, to->row);
                    for (int c = c1; c <= c2; ++c)
                        for (int r = r1; r <= r2; ++r)
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
    int num_cols = std::min(sheet.col_count(), VISIBLE_COLS);
    int num_rows = sheet.row_count();

    if (num_rows < 100) num_rows = 100;

    // Zone detection and cursor setting (uses rect captured from previous frame)
    bool in_formula_or_edit = state.editor.is_formula_mode() || state.editor.is_editing();
    CellZone hovered_zone = CellZone::None;
    if (!in_formula_or_edit && state.drag_mode == CellDragMode::None) {
        ImVec2 mouse = ImGui::GetMousePos();
        hovered_zone = detect_zone(mouse, state.selected_rect_min, state.selected_rect_max);
        switch (hovered_zone) {
            case CellZone::FillHandle: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); break;
            case CellZone::Edge:       ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); break;
            default: break;
        }
    }

    // Drag initiation on mouse press
    if (!in_formula_or_edit && state.drag_mode == CellDragMode::None &&
        ImGui::IsMouseClicked(0) && hovered_zone != CellZone::None) {
        switch (hovered_zone) {
            case CellZone::Edge:
                state.drag_mode = CellDragMode::Move;
                state.drag_source = state.selected;
                break;
            case CellZone::Interior:
                state.drag_mode = CellDragMode::Select;
                state.sel_anchor = state.selected;
                state.has_range_selection = false;
                break;
            case CellZone::FillHandle:
                state.drag_mode = CellDragMode::Fill;
                state.drag_source = state.selected;
                break;
            default: break;
        }
    }

    int table_cols = num_cols + 1;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable;

    if (!ImGui::BeginTable("spreadsheet", table_cols, flags)) return false;

    ImGui::TableSetupScrollFreeze(1, 1);

    ImGui::TableSetupColumn("##rownum", ImGuiTableColumnFlags_WidthFixed, ROW_NUM_WIDTH);
    for (int c = 0; c < num_cols; ++c) {
        ImGui::TableSetupColumn(CellAddress::col_to_letters(c).c_str(),
                                ImGuiTableColumnFlags_WidthFixed, COL_WIDTH);
    }
    ImGui::TableHeadersRow();

    // Track which cell the mouse hovers this frame (for formula drag-end detection)
    CellAddress drag_hover{-1, -1};

    // Cancel drag if we left formula mode (e.g. Escape)
    if (state.formula_dragging && !state.editor.is_formula_mode())
        state.formula_dragging = false;

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
    bool has_formula_drag_highlight = state.formula_dragging && state.formula_drag_target.col >= 0;
    int fd_c1 = 0, fd_c2 = -1, fd_r1 = 0, fd_r2 = -1;
    if (has_formula_drag_highlight) {
        fd_c1 = std::min(state.formula_drag_origin.col, state.formula_drag_target.col);
        fd_c2 = std::max(state.formula_drag_origin.col, state.formula_drag_target.col);
        fd_r1 = std::min(state.formula_drag_origin.row, state.formula_drag_target.row);
        fd_r2 = std::max(state.formula_drag_origin.row, state.formula_drag_target.row);
    }

    bool has_fill_drag_highlight = state.drag_mode == CellDragMode::Fill && state.drag_target.col >= 0;
    int fl_c1 = 0, fl_c2 = -1, fl_r1 = 0, fl_r2 = -1;
    if (has_fill_drag_highlight) {
        fl_c1 = std::min(state.drag_source.col, state.drag_target.col);
        fl_c2 = std::max(state.drag_source.col, state.drag_target.col);
        fl_r1 = std::min(state.drag_source.row, state.drag_target.row);
        fl_r2 = std::max(state.drag_source.row, state.drag_target.row);
    }

    ImGuiListClipper clipper;
    clipper.Begin(num_rows);

    std::string display;
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%d", row + 1);

            for (int col = 0; col < num_cols; ++col) {
                ImGui::TableSetColumnIndex(col + 1);

                CellAddress addr{col, row};
                ImGui::PushID(row * VISIBLE_COLS + col);

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

                // Live highlight of the fill-drag target range
                if (has_fill_drag_highlight &&
                    col >= fl_c1 && col <= fl_c2 && row >= fl_r1 && row <= fl_r2)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                           fill_drag_color);

                bool is_selected = (state.selected == addr);
                // Highlight cells in selection range
                if (!is_selected && state.has_range_selection) {
                    is_selected = (col >= sel_min.col && col <= sel_max.col &&
                                   row >= sel_min.row && row <= sel_max.row);
                }
                bool is_editing = state.editor.is_editing() && (state.editor.editing_cell() == addr);

                if (is_editing) {
                    ImGui::SetNextItemWidth(-1);
                    if (state.editor.render()) {
                        committed = true;
                    }
                } else {
                    CellValue val = sheet.get_value(addr);
                    CellFormat fmt = formats.get(addr);
                    display = format_value(val, fmt);

                    // Capture cell top-left before any cursor adjustments
                    ImVec2 cell_min = ImGui::GetCursorScreenPos();

                    // Right-align numbers
                    if (is_number(val)) {
                        float w = ImGui::CalcTextSize(display.c_str()).x;
                        float avail = ImGui::GetContentRegionAvail().x;
                        if (w < avail) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - w);
                    }

                    // Color errors red
                    if (is_error(val)) ImGui::PushStyleColor(ImGuiCol_Text, error_text_color);

                    if (ImGui::Selectable(display.empty() ? "##empty" : display.c_str(),
                                          is_selected,
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (!state.editor.is_formula_mode() &&
                            state.drag_mode == CellDragMode::None) {
                            state.selected = addr;
                            state.sel_anchor = addr;
                            state.has_range_selection = false;

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

                    // Tooltip for date cells
                    if (ImGui::IsItemHovered() && fmt.type == FormatType::DATE
                        && !fmt.date_input_hint.empty()) {
                        ImGui::SetTooltip("%s", fmt.date_input_hint.c_str());
                    }

                    // Capture selected cell rect using full cell bounds
                    // GetItemRectMax gives correct bottom-right (Selectable spans cell width),
                    // but use cell_min for top-left since right-align may shift cursor.
                    if (state.selected == addr) {
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

                        // Formula-mode drag start
                        if (state.editor.is_formula_mode() && ImGui::IsMouseClicked(0)) {
                            state.formula_drag_origin = addr;
                            state.formula_dragging = true;
                        }
                    }

                    if (is_error(val)) ImGui::PopStyleColor();
                }

                ImGui::PopID();
            }
        }
    }

    ImGui::EndTable();

    // Update formula drag target each frame for visual feedback
    if (state.formula_dragging && ImGui::IsMouseDown(0)) {
        if (drag_hover.col >= 0)
            state.formula_drag_target = drag_hover;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    // Formula-mode drag end: mouse released over a cell
    if (state.formula_dragging && ImGui::IsMouseReleased(0)) {
        if (state.editor.is_formula_mode() && drag_hover.col >= 0) {
            if (state.formula_drag_origin == drag_hover)
                state.editor.insert_ref(drag_hover.to_a1());
            else
                state.editor.insert_ref(
                    state.formula_drag_origin.to_a1() + ":" + drag_hover.to_a1());
        }
        state.formula_dragging = false;
        state.formula_drag_target = {-1, -1};
    }

    // Cell drag tracking while mouse is held
    if (state.drag_mode != CellDragMode::None && ImGui::IsMouseDown(0)) {
        if (drag_hover.col >= 0 && drag_hover.row >= 0) {
            state.drag_target = drag_hover;
            if (state.drag_mode == CellDragMode::Select) {
                state.selected = drag_hover;
                if (!(drag_hover == state.sel_anchor))
                    state.has_range_selection = true;
            }
        }
        // Set cursor during drag
        switch (state.drag_mode) {
            case CellDragMode::Move: ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); break;
            case CellDragMode::Fill: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); break;
            default: break;
        }
    }

    // Drag completion on mouse release
    if (state.drag_mode != CellDragMode::None && ImGui::IsMouseReleased(0)) {
        if (state.drag_mode == CellDragMode::Move || state.drag_mode == CellDragMode::Fill) {
            if (drag_hover.col >= 0 && drag_hover.row >= 0) {
                state.drag_target = drag_hover;
                state.drag_completed = true;
            }
        }
        // Always reset drag mode on mouse release (#5)
        state.drag_mode = CellDragMode::None;
    }

    return committed;
}

}  // namespace magic
