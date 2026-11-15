// Part of MainWindow, split out of MainWindow.cpp purely to keep each translation
// unit readable (issue #17). Same class, same behaviour, and no declaration in
// MainWindow.hpp changed. Modal windows.

#include "ui/MainWindow.hpp"
#include "session/SessionState.hpp"
#include "session/AutoSave.hpp"
#include "core/ConditionalFormat.hpp"
#include "core/FillPattern.hpp"
#include "core/Workbook.hpp"
#include "services/Clipboard.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "util/Sha256.hpp"
#include <nfd.hpp>
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

// ── Shared action methods ────────────────────────────────────────────────────

void MainWindow::render_modals(SessionState& state) {
    // Unsaved changes modal
    if (show_dirty_new_modal_ || wants_close_)
        ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Discard them?");
        ImGui::Spacing();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            if (wants_close_) {
                should_quit_ = true;
                wants_close_ = false;
            } else {
                state.reset_to_new();
            }
            show_dirty_new_modal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            wants_close_ = false;
            show_dirty_new_modal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Paste Special modal (Ctrl+Shift+V)
    if (show_paste_special_)
        ImGui::OpenPopup("Paste Special");
    if (ImGui::BeginPopupModal("Paste Special", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Choose paste mode:");
        ImGui::Spacing();
        if (ImGui::Button("Values Only", ImVec2(140, 0))) {
            action_paste_special(state, PasteMode::ValuesOnly);
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Formulas Only", ImVec2(140, 0))) {
            action_paste_special(state, PasteMode::FormulasOnly);
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Transpose", ImVec2(140, 0))) {
            action_paste_special(state, PasteMode::Transpose);
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            show_paste_special_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Plugin trust confirmation modal
    if (!state.pending_plugin_path.empty()) {
        if (state.pending_plugin_path != trust_modal_path_) {
            trust_modal_path_ = state.pending_plugin_path;
            trust_modal_hash_ = sha256_of_file(state.pending_plugin_path);
        }
        ImGui::OpenPopup("Trust Plugin?");
    }
    if (ImGui::BeginPopupModal("Trust Plugin?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("An untrusted plugin was dropped:");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", state.pending_plugin_path.c_str());
        ImGui::Spacing();
        ImGui::Text("SHA-256:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", trust_modal_hash_.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Trust & Load", ImVec2(140, 0))) {
            state.plugin_manager.trust_and_load(state.pending_plugin_path);
            toasts_.show("Plugin loaded");
            state.pending_plugin_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            state.pending_plugin_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Recovery modal — shown on startup if a recovery file exists
    if (state.show_recovery_modal)
        ImGui::OpenPopup("Recover Unsaved Work?");
    if (ImGui::BeginPopupModal("Recover Unsaved Work?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A recovery file was found from a previous session.");
        ImGui::Text("Would you like to restore it?");
        ImGui::Spacing();
        if (ImGui::Button("Restore", ImVec2(120, 0))) {
            auto path = AutoSave::recovery_path();
            if (state.open_file(path.string())) {
                state.current_file.clear();  // don't associate with recovery file
                toasts_.show("Recovered unsaved work");
            } else {
                toasts_.show("Recovery failed");
            }
            AutoSave::discard_recovery();
            state.show_recovery_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            AutoSave::discard_recovery();
            state.show_recovery_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MainWindow::render_cond_format_editor(SessionState& state) {
    auto& store = state.active_state().cond_format;
    ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Conditional Formatting", &show_cond_format_)) {
        ImGui::End();
        return;
    }

    static const char* op_names[] = {">", "<", ">=", "<=", "==", "!="};

    // Existing rules
    int to_remove = -1;
    for (std::size_t i = 0; i < store.size(); ++i) {
        auto& rule = store.rules()[i];
        ImGui::PushID(static_cast<int>(i));

        // Column selector
        ImGui::SetNextItemWidth(60);
        char col_label[8];
        if (rule.column == -1)
            std::snprintf(col_label, sizeof(col_label), "All");
        else
            std::snprintf(col_label, sizeof(col_label), "%s",
                          CellAddress::col_to_letters(rule.column).c_str());
        if (ImGui::BeginCombo("##col", col_label)) {
            if (ImGui::Selectable("All", rule.column == -1))
                rule.column = -1;
            for (int32_t c = 0; c < 26; ++c) {
                auto lbl = CellAddress::col_to_letters(c);
                if (ImGui::Selectable(lbl.c_str(), rule.column == c))
                    rule.column = c;
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        int op_idx = static_cast<int>(rule.op);
        if (ImGui::Combo("##op", &op_idx, op_names, 6))
            rule.op = static_cast<ConditionOp>(op_idx);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputDouble("##val", &rule.threshold, 0, 0, "%.2f");

        ImGui::SameLine();
        float color[4] = {rule.color.r / 255.0f, rule.color.g / 255.0f,
                           rule.color.b / 255.0f, rule.color.a / 255.0f};
        ImGui::SetNextItemWidth(40);
        if (ImGui::ColorEdit4("##clr", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            rule.color = {static_cast<uint8_t>(color[0] * 255),
                          static_cast<uint8_t>(color[1] * 255),
                          static_cast<uint8_t>(color[2] * 255),
                          static_cast<uint8_t>(color[3] * 255)};
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
            to_remove = static_cast<int>(i);

        ImGui::PopID();
    }

    if (to_remove >= 0)
        store.remove_rule(static_cast<std::size_t>(to_remove));

    ImGui::Separator();
    if (ImGui::Button("Add Rule")) {
        ConditionalRule rule;
        rule.column = grid_state_.selection.selected_cell.col;
        store.add_rule(rule);
    }

    ImGui::End();
}

}  // namespace magic
