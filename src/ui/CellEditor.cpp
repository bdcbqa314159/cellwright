#include "ui/CellEditor.hpp"
#include "formula/FunctionRegistry.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

void CellEditor::begin_edit(const CellAddress& cell, const std::string& initial, bool select_all) {
    editing_ = true;
    focus_needed_ = true;
    select_all_ = select_all;
    cursor_to_end_ = !select_all && !initial.empty();
    frames_since_start_ = 0;
    cursor_pos_ = 0;
    cell_ = cell;
    std::strncpy(buf_, initial.c_str(), sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
    autocomplete_.reset();
}

bool CellEditor::render(const FunctionRegistry* registry) {
    if (!editing_) return false;

    // Only grab focus on the first frame
    if (focus_needed_) {
        ImGui::SetKeyboardFocusHere();
        focus_needed_ = false;
    }

    ImGui::SetNextItemWidth(-1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackAlways;
    if (select_all_)
        flags |= ImGuiInputTextFlags_AutoSelectAll;

    // Callback to track cursor position (and handle cursor_to_end_)
    struct CbData {
        int* cursor_pos;
        bool* cursor_to_end;
    };
    CbData cb_data{&cursor_pos_, &cursor_to_end_};

    auto cb = [](ImGuiInputTextCallbackData* data) -> int {
        auto* ud = static_cast<CbData*>(data->UserData);
        *ud->cursor_pos = data->CursorPos;
        if (*ud->cursor_to_end) {
            data->CursorPos = data->BufTextLen;
            data->SelectionStart = data->BufTextLen;
            data->SelectionEnd = data->BufTextLen;
            *ud->cursor_to_end = false;
        }
        return 0;
    };

    bool committed = ImGui::InputText("##celledit", buf_, sizeof(buf_), flags, cb, &cb_data);
    ImVec2 anchor = ImGui::GetItemRectMin();
    anchor.y = ImGui::GetItemRectMax().y;

    ++frames_since_start_;

    // Autocomplete popup
    bool ac_consumed = false;
    if (registry && buf_[0] == '=') {
        std::string insertion;
        if (autocomplete_.render(buf_, cursor_pos_, *registry, anchor, insertion)) {
            // Insert completion at cursor position
            std::size_t len = std::strlen(buf_);
            if (len + insertion.size() < sizeof(buf_) - 1) {
                std::strncat(buf_, insertion.c_str(), sizeof(buf_) - 1 - len);
            }
            cursor_to_end_ = true;
            focus_needed_ = true;
            frames_since_start_ = 0;
            ac_consumed = true;
        }
    }

    // Escape cancels
    if (!ac_consumed && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        editing_ = false;
        autocomplete_.reset();
        return false;
    }

    // Commit on Enter
    if (committed) {
        editing_ = false;
        autocomplete_.reset();
        return true;
    }

    // Commit on lost focus — but only after a few frames to let focus settle.
    // In formula mode, suppress: clicks insert cell refs, only Enter/Escape end editing.
    if (frames_since_start_ > 3 && buf_[0] != '=') {
        if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
            editing_ = false;
            autocomplete_.reset();
            return true;
        }
    }

    return false;
}

void CellEditor::cancel() {
    editing_ = false;
    autocomplete_.reset();
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
