#include "ui/AutocompletePopup.hpp"
#include "formula/FunctionRegistry.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace magic {

std::string AutocompletePopup::extract_token(const char* buf, int cursor_pos) {
    if (!buf || cursor_pos <= 0) return {};
    // Must be a formula (starts with =)
    if (buf[0] != '=') return {};

    // Scan backward from cursor to find start of alphanumeric token
    int end = cursor_pos;
    int start = end;
    while (start > 0 && std::isalpha(static_cast<unsigned char>(buf[start - 1])))
        --start;

    if (start >= end) return {};
    // Must be preceded by =, (, ,, +, -, *, /, ^, <, >, or space (function name context)
    if (start > 0) {
        char prev = buf[start - 1];
        if (prev != '=' && prev != '(' && prev != ',' && prev != '+' &&
            prev != '-' && prev != '*' && prev != '/' && prev != '^' &&
            prev != '<' && prev != '>' && prev != ' ')
            return {};
    }

    std::string token(buf + start, end - start);
    // Uppercase for matching
    for (auto& ch : token)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return token;
}

std::string AutocompletePopup::find_enclosing_function(const char* buf, int cursor_pos) {
    if (!buf || cursor_pos <= 0) return {};
    if (buf[0] != '=') return {};

    // Walk backwards from cursor, tracking paren depth.
    // When we find an unmatched '(' preceded by a function name, return it.
    int depth = 0;
    for (int i = cursor_pos - 1; i >= 0; --i) {
        char c = buf[i];
        if (c == ')') ++depth;
        else if (c == '(') {
            if (depth > 0) { --depth; continue; }
            // Found unmatched '(' — scan backwards for function name
            int end = i;
            int start = end;
            while (start > 0 && std::isalpha(static_cast<unsigned char>(buf[start - 1])))
                --start;
            if (start < end) {
                std::string name(buf + start, end - start);
                for (auto& ch : name)
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                return name;
            }
            return {};
        }
    }
    return {};
}

void AutocompletePopup::update_matches(const std::string& prefix,
                                        const FunctionRegistry& registry) {
    if (prefix == prefix_ && !matches_.empty()) return;
    prefix_ = prefix;
    matches_.clear();
    if (prefix.empty()) return;

    for (auto& [name, _] : registry.all()) {
        if (name.size() >= prefix.size() &&
            name.compare(0, prefix.size(), prefix) == 0) {
            matches_.push_back(name);
        }
    }
    std::sort(matches_.begin(), matches_.end());
    if (matches_.size() > 10)
        matches_.resize(10);
    selected_ = 0;
}

void AutocompletePopup::reset() {
    active_ = false;
    selected_ = 0;
    prefix_.clear();
    matches_.clear();
}

bool AutocompletePopup::render(const char* buf, int cursor_pos,
                                const FunctionRegistry& registry,
                                ImVec2 anchor_pos, std::string& out_text) {
    std::string token = extract_token(buf, cursor_pos);
    if (token.empty() || token.size() < 1) {
        active_ = false;
        matches_.clear();
        return false;
    }

    update_matches(token, registry);
    if (matches_.empty()) {
        active_ = false;
        return false;
    }

    // Don't show if the token exactly matches a function name (already typed fully)
    if (matches_.size() == 1 && matches_[0] == token) {
        active_ = false;
        matches_.clear();
        return false;
    }

    active_ = true;

    // Handle keyboard: Up/Down to navigate, Tab/Enter to accept, Escape to dismiss
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
        selected_ = std::min(selected_ + 1, static_cast<int>(matches_.size()) - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
        selected_ = std::max(selected_ - 1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        reset();
        return false;
    }
    bool accepted = ImGui::IsKeyPressed(ImGuiKey_Tab, false);

    // Draw the popup
    ImGui::SetNextWindowPos(ImVec2(anchor_pos.x, anchor_pos.y));
    ImGui::SetNextWindowSize(ImVec2(180, 0));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("##autocomplete", nullptr, flags)) {
        for (int i = 0; i < static_cast<int>(matches_.size()); ++i) {
            bool is_selected = (i == selected_);
            if (ImGui::Selectable(matches_[i].c_str(), is_selected)) {
                selected_ = i;
                accepted = true;
            }
        }
    }
    ImGui::End();

    if (accepted && selected_ >= 0 && selected_ < static_cast<int>(matches_.size())) {
        // Return the remaining characters + "("
        const auto& match = matches_[selected_];
        out_text = match.substr(token.size()) + "(";
        reset();
        return true;
    }

    return false;
}

}  // namespace magic
