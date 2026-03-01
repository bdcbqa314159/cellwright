#include "ui/CellEditor.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

void CellEditor::begin_edit(const CellAddress& cell, const std::string& initial) {
    editing_ = true;
    cell_ = cell;
    std::strncpy(buf_, initial.c_str(), sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
}

bool CellEditor::render() {
    if (!editing_) return false;

    ImGui::SetKeyboardFocusHere();
    bool committed = ImGui::InputText("##celledit", buf_, sizeof(buf_),
                                       ImGuiInputTextFlags_EnterReturnsTrue |
                                       ImGuiInputTextFlags_AutoSelectAll);

    if (committed || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        bool result = committed;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) result = false;
        editing_ = false;
        return result;
    }

    return false;
}

void CellEditor::cancel() {
    editing_ = false;
}

}  // namespace magic
