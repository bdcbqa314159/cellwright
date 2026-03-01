#include "ui/StyleSetup.hpp"
#include <imgui.h>

namespace magic {

void setup_style() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowRounding = 4.0f;
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(6, 4);

    // Spreadsheet-friendly colors
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);
}

}  // namespace magic
