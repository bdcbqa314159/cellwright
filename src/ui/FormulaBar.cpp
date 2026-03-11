#include "ui/FormulaBar.hpp"
#include "core/Sheet.hpp"
#include "core/CellValue.hpp"
#include "formula/FunctionRegistry.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

bool FormulaBar::render(Sheet& sheet, const CellAddress& selected,
                         bool cell_editing, const FunctionRegistry* registry) {
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

    // Refresh buffer when selection changes (and not currently editing in cell)
    if (!(selected == last_selected_) && !cell_editing) {
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

    // Read-only when cell editor is active to prevent focus stealing
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackAlways;
    if (cell_editing)
        flags |= ImGuiInputTextFlags_ReadOnly;

    // Red tint on formula bar when the cell contains an error
    CellValue val = sheet.get_value(selected);
    bool has_error = is_error(val);
    if (has_error) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    }

    // Callback to track cursor position
    auto cb = [](ImGuiInputTextCallbackData* data) -> int {
        *static_cast<int*>(data->UserData) = data->CursorPos;
        return 0;
    };

    if (ImGui::InputText("##formula", buf_, sizeof(buf_), flags, cb, &cursor_pos_)) {
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

    editing_ = !cell_editing && ImGui::IsItemActive();

    // Autocomplete popup (only when formula bar is active and in formula mode)
    if (registry && editing_ && buf_[0] == '=') {
        std::string insertion;
        if (autocomplete_.render(buf_, cursor_pos_, *registry, anchor, insertion)) {
            std::size_t len = std::strlen(buf_);
            if (len + insertion.size() < sizeof(buf_) - 1) {
                std::strncat(buf_, insertion.c_str(), sizeof(buf_) - 1 - len);
            }
        }
    } else {
        autocomplete_.reset();
    }

    return committed;
}

}  // namespace magic
