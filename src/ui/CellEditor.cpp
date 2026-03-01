#include "ui/CellEditor.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

void CellEditor::begin_edit(const CellAddress& cell, const std::string& initial, bool select_all) {
    editing_ = true;
    focus_needed_ = true;
    select_all_ = select_all;
    cursor_to_end_ = !select_all && !initial.empty();
    frames_since_start_ = 0;
    cell_ = cell;
    std::strncpy(buf_, initial.c_str(), sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
}

bool CellEditor::render() {
    if (!editing_) return false;

    // Only grab focus on the first frame
    if (focus_needed_) {
        ImGui::SetKeyboardFocusHere();
        focus_needed_ = false;
    }

    ImGui::SetNextItemWidth(-1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (select_all_)
        flags |= ImGuiInputTextFlags_AutoSelectAll;

    // When type-to-edit seeds the buffer, SetKeyboardFocusHere selects all text
    // on activation. Use a callback to force cursor to end with no selection.
    ImGuiInputTextCallback cb = nullptr;
    void* cb_data = nullptr;
    if (cursor_to_end_) {
        flags |= ImGuiInputTextFlags_CallbackAlways;
        cb = [](ImGuiInputTextCallbackData* data) -> int {
            bool* flag = static_cast<bool*>(data->UserData);
            if (*flag) {
                data->CursorPos = data->BufTextLen;
                data->SelectionStart = data->BufTextLen;
                data->SelectionEnd = data->BufTextLen;
                *flag = false;
            }
            return 0;
        };
        cb_data = &cursor_to_end_;
    }

    bool committed = ImGui::InputText("##celledit", buf_, sizeof(buf_), flags, cb, cb_data);

    ++frames_since_start_;

    // Escape cancels
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        editing_ = false;
        return false;
    }

    // Commit on Enter
    if (committed) {
        editing_ = false;
        return true;
    }

    // Commit on lost focus — but only after a few frames to let focus settle.
    // In formula mode, suppress: clicks insert cell refs, only Enter/Escape end editing.
    if (frames_since_start_ > 3 && buf_[0] != '=') {
        if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
            editing_ = false;
            return true;
        }
    }

    return false;
}

void CellEditor::cancel() {
    editing_ = false;
}

void CellEditor::insert_ref(const std::string& ref) {
    std::size_t len = std::strlen(buf_);
    if (len + ref.size() < sizeof(buf_) - 1) {
        std::strncat(buf_, ref.c_str(), sizeof(buf_) - 1 - len);
    }
    focus_needed_ = true;
    cursor_to_end_ = true;
    frames_since_start_ = 0;
}

}  // namespace magic
