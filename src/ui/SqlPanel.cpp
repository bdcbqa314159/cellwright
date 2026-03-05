#include "ui/SqlPanel.hpp"
#include "core/Sheet.hpp"
#include <imgui.h>

namespace magic {

void SqlPanel::render(DuckDBEngine& engine, Sheet& sheet) {
    if (!visible_) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("SQL Query", &visible_)) {
        ImGui::End();
        return;
    }

    // Import button
    if (ImGui::Button("Import Sheet"))
        engine.import_sheet(sheet, "data");
    ImGui::SameLine();
    ImGui::TextDisabled("(imports active sheet as 'data' table)");

    // SQL input
    ImGui::InputTextMultiline("##sql", sql_buf_, sizeof(sql_buf_),
                               ImVec2(-1, ImGui::GetTextLineHeight() * 5));

    // Execute / Export buttons
    if (ImGui::Button("Execute")) {
        engine.import_sheet(sheet, "data");
        last_result_ = engine.query(sql_buf_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export to Sheet") && last_result_.ok()) {
        DuckDBEngine::export_to_sheet(last_result_, sheet);
    }

    // Error display
    if (!last_result_.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
        ImGui::TextWrapped("Error: %s", last_result_.error.c_str());
        ImGui::PopStyleColor();
    }

    // Results table
    if (last_result_.ok() && last_result_.col_count() > 0) {
        ImGui::Text("Rows: %zu", last_result_.row_count());

        int ncols = static_cast<int>(last_result_.col_count());
        if (ImGui::BeginTable("##sql_results", ncols,
                               ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_Resizable,
                               ImVec2(0, 0))) {
            // Headers
            for (int c = 0; c < ncols; ++c)
                ImGui::TableSetupColumn(last_result_.column_names[c].c_str());
            ImGui::TableHeadersRow();

            // Data rows
            size_t nrows = last_result_.row_count();
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(nrows));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    ImGui::TableNextRow();
                    for (int c = 0; c < ncols; ++c) {
                        ImGui::TableSetColumnIndex(c);
                        const auto& val = last_result_.columns[c][row];
                        std::string display = to_display_string(val);
                        ImGui::TextUnformatted(display.c_str());
                    }
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
}

}  // namespace magic
