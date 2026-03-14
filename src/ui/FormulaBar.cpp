#include "ui/FormulaBar.hpp"
#include "core/Sheet.hpp"
#include "core/CellValue.hpp"
#include "formula/FunctionRegistry.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

bool FormulaBar::render(Sheet& sheet, const CellAddress& selected,
                         bool cell_editing, const FunctionRegistry* registry,
                         ImFont* mono_font, const char* cell_editor_buf) {
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
        std::snprintf(name_buf_, sizeof(name_buf_), "%s", cell_label.c_str());
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

    // Mirror cell editor buffer into formula bar while editing in-cell
    // (but not if the formula bar itself is active — user is typing here).
    if (cell_editing && cell_editor_buf && !editing_) {
        std::snprintf(buf_, sizeof(buf_), "%s", cell_editor_buf);
    } else if (!(selected == last_selected_) && !cell_editing) {
        last_selected_ = selected;
        std::string content;
        if (sheet.has_formula(selected)) {
            content = "=" + sheet.get_formula(selected);
        } else {
            content = to_display_string(sheet.get_value(selected));
        }
        std::snprintf(buf_, sizeof(buf_), "%s", content.c_str());
    }

    ImGui::SetNextItemWidth(-1);

    // Use monospace font for formula editing
    if (mono_font) ImGui::PushFont(mono_font);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackAlways;

    // Red tint on formula bar when the cell contains an error
    CellValue val = sheet.get_value(selected);
    bool has_error = is_error(val);
    if (has_error) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    }

    // Callback to track cursor position and handle forced positioning
    struct CbData {
        int* cursor_pos;
        int* set_cursor_pos;
    };
    CbData cb_data{&cursor_pos_, &set_cursor_pos_};

    auto cb = [](ImGuiInputTextCallbackData* data) -> int {
        auto* ud = static_cast<CbData*>(data->UserData);
        if (*ud->set_cursor_pos >= 0) {
            data->CursorPos = *ud->set_cursor_pos;
            data->SelectionStart = *ud->set_cursor_pos;
            data->SelectionEnd = *ud->set_cursor_pos;
            *ud->set_cursor_pos = -1;
        }
        *ud->cursor_pos = data->CursorPos;
        return 0;
    };

    if (ImGui::InputText("##formula", buf_, sizeof(buf_), flags, cb, &cb_data)) {
        committed = true;
    }

    ImVec2 anchor = ImGui::GetItemRectMin();
    anchor.y = ImGui::GetItemRectMax().y;

    // Tooltip showing the error type
    if (has_error && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", to_display_string(val).c_str());
    }

    if (has_error)
        ImGui::PopStyleColor(2);

    if (mono_font) ImGui::PopFont();

    bool bar_active = ImGui::IsItemActive();
    // If formula bar gains focus while cell editor was active, signal takeover
    if (bar_active && cell_editing && !editing_) {
        took_focus_ = true;
    }
    editing_ = bar_active;

    // Function signature tooltip (when cursor is inside a function call)
    if (registry && editing_ && buf_[0] == '=' && !autocomplete_.is_active()) {
        auto func_name = AutocompletePopup::find_enclosing_function(buf_, cursor_pos_);
        if (!func_name.empty()) {
            auto sig = registry->signature(func_name);
            if (!sig.empty()) {
                ImGui::SetNextWindowPos(ImVec2(anchor.x, anchor.y + 2));
                ImGuiWindowFlags tip_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.2f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
                if (ImGui::Begin("##sig_tooltip", nullptr, tip_flags)) {
                    ImGui::TextUnformatted(sig.c_str());
                }
                ImGui::End();
                ImGui::PopStyleColor(2);
            }
        }
    }

    // Autocomplete popup (only when formula bar is active and in formula mode)
    if (registry && editing_ && buf_[0] == '=') {
        std::string insertion;
        if (autocomplete_.render(buf_, cursor_pos_, *registry, anchor, insertion)) {
            std::size_t len = std::strlen(buf_);
            std::size_t insert_pos = static_cast<std::size_t>(cursor_pos_);
            if (len + insertion.size() < sizeof(buf_) - 1) {
                // Shift existing text after cursor to make room, then insert
                std::memmove(buf_ + insert_pos + insertion.size(),
                             buf_ + insert_pos,
                             len - insert_pos + 1); // +1 for null terminator
                std::memcpy(buf_ + insert_pos, insertion.c_str(), insertion.size());
            }
        }
    } else {
        autocomplete_.reset();
    }

    return committed;
}

void FormulaBar::insert_ref(const std::string& ref) {
    std::size_t len = std::strlen(buf_);
    std::size_t insert_pos = static_cast<std::size_t>(cursor_pos_);
    if (insert_pos > len) insert_pos = len;
    if (len + ref.size() < sizeof(buf_) - 1) {
        std::memmove(buf_ + insert_pos + ref.size(),
                     buf_ + insert_pos,
                     len - insert_pos + 1);
        std::memcpy(buf_ + insert_pos, ref.c_str(), ref.size());
        int new_pos = static_cast<int>(insert_pos + ref.size());
        cursor_pos_ = new_pos;
        set_cursor_pos_ = new_pos;
    }
}

}  // namespace magic
