#include "ui/CellEditor.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

void CellEditor::begin_edit(const CellAddress& cell, const std::string& initial) {
    editing_ = true;
    focus_needed_ = true;
    cell_ = cell;
    std::strncpy(buf_, initial.c_str(), sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
}

bool CellEditor::render() {
    if (!editing_) return false;

    // Only grab focus on the first frame — otherwise we fight with the InputText
    if (focus_needed_) {
        ImGui::SetKeyboardFocusHere();
        focus_needed_ = false;
    }

    ImGui::SetNextItemWidth(-1);
    bool committed = ImGui::InputText("##celledit", buf_, sizeof(buf_),
                                       ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_AutoSelectAll);

    // Also commit if we lose focus (clicked elsewhere)
    bool lost_focus = !ImGui::IsItemActive() && !ImGui::IsItemFocused() && !focus_needed_;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        editing_ = false;
        return false;
    }

    if (committed || lost_focus) {
        editing_ = false;
        return true;
    }

    return false;
}

void CellEditor::cancel() {
    editing_ = false;
}

}  // namespace magic
