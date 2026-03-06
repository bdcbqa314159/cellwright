#include "ui/FindBar.hpp"
#include "core/Sheet.hpp"
#include "core/Command.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>


namespace magic {

// Case-insensitive search. Returns iterator to match start, or haystack.end().
static std::string::const_iterator icase_find(const std::string& haystack, const std::string& needle) {
    return std::search(haystack.begin(), haystack.end(),
                       needle.begin(), needle.end(),
                       [](char a, char b) {
                           return std::tolower(static_cast<unsigned char>(a)) ==
                                  std::tolower(static_cast<unsigned char>(b));
                       });
}

static bool icase_contains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return false;
    return icase_find(haystack, needle) != haystack.end();
}

void FindBar::do_search(Sheet& sheet) {
    matches_.clear();
    match_idx_ = -1;
    std::string query(search_buf_);
    if (query.empty()) return;
    last_search_ = query;

    for (int32_t r = 0; r < sheet.row_count(); ++r) {
        for (int32_t c = 0; c < sheet.col_count(); ++c) {
            CellAddress addr{c, r};
            CellValue val = sheet.get_value(addr);
            std::string display = to_display_string(val);
            if (icase_contains(display, query)) {
                matches_.push_back(addr);
                continue;
            }
            if (sheet.has_formula(addr)) {
                if (icase_contains(sheet.get_formula(addr), query))
                    matches_.push_back(addr);
            }
        }
    }
    if (!matches_.empty()) {
        match_idx_ = 0;
        nav_target_ = matches_[0];
        has_nav_ = true;
    }
}

void FindBar::find_next() {
    if (matches_.empty()) return;
    match_idx_ = (match_idx_ + 1) % static_cast<int>(matches_.size());
    nav_target_ = matches_[match_idx_];
    has_nav_ = true;
}

void FindBar::find_prev() {
    if (matches_.empty()) return;
    match_idx_ = (match_idx_ - 1 + static_cast<int>(matches_.size())) % static_cast<int>(matches_.size());
    nav_target_ = matches_[match_idx_];
    has_nav_ = true;
}

void FindBar::render(Sheet& sheet, UndoManager* undo) {
    if (!visible_) return;

    ImGui::PushID("FindBar");

    ImGui::SetNextItemWidth(200);
    bool search_enter = ImGui::InputText("##search", search_buf_, sizeof(search_buf_),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    if (search_enter) {
        if (std::string(search_buf_) != last_search_)
            do_search(sheet);
        else
            find_next();
    }

    ImGui::SameLine();
    if (ImGui::Button("Find Next")) {
        if (std::string(search_buf_) != last_search_)
            do_search(sheet);
        else
            find_next();
    }
    ImGui::SameLine();
    if (ImGui::Button("Find Prev")) {
        if (std::string(search_buf_) != last_search_)
            do_search(sheet);
        else
            find_prev();
    }

    if (!matches_.empty()) {
        ImGui::SameLine();
        ImGui::Text("%d/%d", match_idx_ + 1, static_cast<int>(matches_.size()));
    }

    if (show_replace_) {
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##replace", replace_buf_, sizeof(replace_buf_));
        ImGui::SameLine();
        if (ImGui::Button("Replace")) {
            if (match_idx_ >= 0 && match_idx_ < static_cast<int>(matches_.size())) {
                CellAddress addr = matches_[match_idx_];
                CellValue val = sheet.get_value(addr);
                std::string text = to_display_string(val);
                std::string needle(search_buf_);
                std::string replacement(replace_buf_);
                auto it = icase_find(text, needle);
                if (it != text.end()) {
                    auto pos = static_cast<size_t>(it - text.cbegin());
                    text.replace(pos, needle.size(), replacement);
                    CellValue new_val{text};
                    if (undo) {
                        std::string old_formula = sheet.has_formula(addr) ? sheet.get_formula(addr) : "";
                        auto cmd = std::make_unique<SetValueCommand>(addr, new_val, val, old_formula);
                        undo->execute(std::move(cmd), sheet);
                    } else {
                        sheet.set_value(addr, new_val);
                        sheet.clear_formula(addr);
                    }
                }
                do_search(sheet);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Replace All")) {
            std::string needle(search_buf_);
            std::string replacement(replace_buf_);
            for (auto& addr : matches_) {
                CellValue val = sheet.get_value(addr);
                std::string text = to_display_string(val);
                auto it = icase_find(text, needle);
                if (it != text.end()) {
                    auto pos = static_cast<size_t>(it - text.cbegin());
                    text.replace(pos, needle.size(), replacement);
                    CellValue new_val{text};
                    if (undo) {
                        std::string old_formula = sheet.has_formula(addr) ? sheet.get_formula(addr) : "";
                        auto cmd = std::make_unique<SetValueCommand>(addr, new_val, val, old_formula);
                        undo->execute(std::move(cmd), sheet);
                    } else {
                        sheet.set_value(addr, new_val);
                        sheet.clear_formula(addr);
                    }
                }
            }
            do_search(sheet);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("X")) {
        hide();
    }

    ImGui::PopID();
}

}  // namespace magic
