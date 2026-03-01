#include "ui/FormulaBar.hpp"
#include "core/Sheet.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

bool FormulaBar::render(Sheet& sheet, const CellAddress& selected, bool cell_editing) {
    bool committed = false;

    std::string cell_label = selected.to_a1();
    ImGui::SetNextItemWidth(60);
    ImGui::InputText("##celladdr", cell_label.data(), cell_label.size(),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();

    // Refresh buffer when selection changes (and not currently editing in cell)
    if (!(selected == last_selected_) && !cell_editing) {
        last_selected_ = selected;
        std::string content;
        if (sheet.has_formula(selected)) {
            content = "=" + sheet.get_formula(selected);
        } else {
            content = to_display_string(sheet.get_value(selected));
        }
        std::strncpy(buf_, content.c_str(), sizeof(buf_) - 1);
        buf_[sizeof(buf_) - 1] = '\0';
    }

    ImGui::SetNextItemWidth(-1);

    // Read-only when cell editor is active to prevent focus stealing
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (cell_editing)
        flags |= ImGuiInputTextFlags_ReadOnly;

    if (ImGui::InputText("##formula", buf_, sizeof(buf_), flags)) {
        committed = true;
    }
    editing_ = !cell_editing && ImGui::IsItemActive();

    return committed;
}

}  // namespace magic
