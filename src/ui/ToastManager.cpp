#include "ui/ToastManager.hpp"
#include <imgui.h>

namespace magic {

void ToastManager::show(const std::string& message, float duration_sec) {
    float now = static_cast<float>(ImGui::GetTime());
    toasts_.push_back({message, now, duration_sec});
}

void ToastManager::render() {
    if (toasts_.empty()) return;

    float now = static_cast<float>(ImGui::GetTime());

    // Remove expired toasts
    while (!toasts_.empty()) {
        auto& front = toasts_.front();
        if (now - front.start_time >= front.duration)
            toasts_.pop_front();
        else
            break;
    }

    if (toasts_.empty()) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float padding = 16.0f;
    float y_offset = padding;

    for (size_t i = 0; i < toasts_.size(); ++i) {
        auto& t = toasts_[i];
        float elapsed = now - t.start_time;
        float remaining = t.duration - elapsed;

        // Fade out during last 0.5s
        float alpha = 1.0f;
        if (remaining < 0.5f)
            alpha = remaining / 0.5f;

        ImGui::SetNextWindowBgAlpha(0.85f * alpha);
        ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x + vp->WorkSize.x - padding,
                   vp->WorkPos.y + vp->WorkSize.y - y_offset),
            ImGuiCond_Always,
            ImVec2(1.0f, 1.0f)  // anchor bottom-right
        );

        char label[32];
        snprintf(label, sizeof(label), "##toast_%zu", i);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, alpha));
        if (ImGui::Begin(label, nullptr,
                         ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoDocking)) {
            ImGui::TextUnformatted(t.message.c_str());
        }
        float win_h = ImGui::GetWindowSize().y;
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        y_offset += win_h + 4.0f;
    }
}

}  // namespace magic
