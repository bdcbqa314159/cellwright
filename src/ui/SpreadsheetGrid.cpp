#include "ui/SpreadsheetGrid.hpp"
#include "core/Sheet.hpp"
#include <imgui.h>

namespace magic {

bool SpreadsheetGrid::render(Sheet& sheet, GridState& state) {
    bool committed = false;
    int num_cols = std::min(sheet.col_count(), VISIBLE_COLS);
    int num_rows = sheet.row_count();

    // Ensure we show at least a reasonable grid
    if (num_rows < 100) num_rows = 100;

    int table_cols = num_cols + 1;  // +1 for row numbers
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable;

    if (!ImGui::BeginTable("spreadsheet", table_cols, flags)) return false;

    // Freeze first column (row numbers) and first row (headers)
    ImGui::TableSetupScrollFreeze(1, 1);

    // Column headers
    ImGui::TableSetupColumn("##rownum", ImGuiTableColumnFlags_WidthFixed, ROW_NUM_WIDTH);
    for (int c = 0; c < num_cols; ++c) {
        ImGui::TableSetupColumn(CellAddress::col_to_letters(c).c_str(),
                                ImGuiTableColumnFlags_WidthFixed, COL_WIDTH);
    }
    ImGui::TableHeadersRow();

    // Virtual scrolling with clipper
    ImGuiListClipper clipper;
    clipper.Begin(num_rows);

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            ImGui::TableNextRow();

            // Row number column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%d", row + 1);

            // Data columns
            for (int col = 0; col < num_cols; ++col) {
                ImGui::TableSetColumnIndex(col + 1);

                CellAddress addr{col, row};
                ImGui::PushID(row * VISIBLE_COLS + col);

                bool is_selected = (state.selected == addr);
                bool is_editing = state.editor.is_editing() && (state.editor.editing_cell() == addr);

                if (is_editing) {
                    // In-cell editor
                    ImGui::SetNextItemWidth(-1);
                    if (state.editor.render()) {
                        committed = true;
                    }
                } else {
                    // Display value
                    CellValue val = sheet.get_value(addr);
                    std::string display = to_display_string(val);

                    // Right-align numbers
                    if (is_number(val)) {
                        float w = ImGui::CalcTextSize(display.c_str()).x;
                        float avail = ImGui::GetContentRegionAvail().x;
                        if (w < avail) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - w);
                    }

                    if (ImGui::Selectable(display.empty() ? "##empty" : display.c_str(),
                                          is_selected,
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selected = addr;

                        // Double click → start editing
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

                ImGui::PopID();
            }
        }
    }

    ImGui::EndTable();
    return committed;
}

}  // namespace magic
