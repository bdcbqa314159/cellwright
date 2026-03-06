#include "ui/FormulaBar.hpp"
#include "core/Sheet.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

bool FormulaBar::render(Sheet& sheet, const CellAddress& selected, bool cell_editing) {
    bool committed = false;

    // Clickable name box — editable InputText for Go-To navigation
    // Only overwrite buffer when the widget is not focused, so the user can type
    ImGui::SetNextItemWidth(60);
    if (focus_name_box_) {
        ImGui::SetKeyboardFocusHere();
        focus_name_box_ = false;
        name_box_active_ = true;
    }
    if (!name_box_active_) {
        std::string cell_label = selected.to_a1();
        std::strncpy(name_buf_, cell_label.c_str(), sizeof(name_buf_) - 1);
        name_buf_[sizeof(name_buf_) - 1] = '\0';
    }
    if (ImGui::InputText("##celladdr", name_buf_, sizeof(name_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto addr = CellAddress::from_a1(std::string(name_buf_));
        if (addr) {
            nav_target_ = *addr;
            has_nav_target_ = true;
        }
        name_box_active_ = false;
    }
    name_box_active_ = ImGui::IsItemActive();
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
