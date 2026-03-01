#include "ui/FormulaBar.hpp"
#include "core/Sheet.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

bool FormulaBar::render(Sheet& sheet, const CellAddress& selected) {
    bool committed = false;

    std::string cell_label = selected.to_a1();
    ImGui::SetNextItemWidth(60);
    ImGui::InputText("##celladdr", cell_label.data(), cell_label.size(),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();

    // Show formula if exists, otherwise show value
    static char buf[1024] = {};
    static CellAddress last_selected{-1, -1};

    if (!(selected == last_selected)) {
        last_selected = selected;
        std::string content;
        if (sheet.has_formula(selected)) {
            content = "=" + sheet.get_formula(selected);
        } else {
            content = to_display_string(sheet.get_value(selected));
        }
        std::strncpy(buf, content.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##formula", buf, sizeof(buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        committed = true;
    }

    return committed;
}

}  // namespace magic
