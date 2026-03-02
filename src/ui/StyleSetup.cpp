#include "ui/StyleSetup.hpp"
#include <imgui.h>

namespace magic {

void setup_style() {
    auto& style = ImGui::GetStyle();
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowRounding = 4.0f;
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(6, 4);
}

void apply_theme(Theme theme) {
    auto& colors = ImGui::GetStyle().Colors;

    if (theme == Theme::Dark) {
        colors[ImGuiCol_WindowBg]          = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        colors[ImGuiCol_ChildBg]           = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        colors[ImGuiCol_PopupBg]           = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
        colors[ImGuiCol_Text]              = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
        colors[ImGuiCol_Border]            = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
        colors[ImGuiCol_FrameBg]           = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.26f, 0.26f, 0.30f, 1.0f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
        colors[ImGuiCol_MenuBarBg]         = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
        colors[ImGuiCol_ScrollbarBg]       = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
        colors[ImGuiCol_Header]            = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.26f, 0.26f, 0.30f, 1.0f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
        colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.28f, 0.28f, 0.32f, 1.0f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.34f, 0.34f, 0.38f, 1.0f);
        colors[ImGuiCol_Tab]              = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
        colors[ImGuiCol_TabHovered]       = ImVec4(0.26f, 0.26f, 0.30f, 1.0f);
        colors[ImGuiCol_TabSelected]      = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
        colors[ImGuiCol_Separator]         = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
        colors[ImGuiCol_TableHeaderBg]     = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
        colors[ImGuiCol_TableBorderLight]  = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
        colors[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    } else {
        // Light theme — Excel/Sheets inspired
        colors[ImGuiCol_WindowBg]          = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);
        colors[ImGuiCol_ChildBg]           = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);
        colors[ImGuiCol_PopupBg]           = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        colors[ImGuiCol_Text]              = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
        colors[ImGuiCol_Border]            = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
        colors[ImGuiCol_FrameBg]           = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.90f, 0.92f, 0.96f, 1.0f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.82f, 0.86f, 0.94f, 1.0f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.82f, 0.82f, 0.86f, 1.0f);
        colors[ImGuiCol_MenuBarBg]         = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
        colors[ImGuiCol_ScrollbarBg]       = ImVec4(0.94f, 0.94f, 0.96f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.72f, 0.72f, 0.76f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
        colors[ImGuiCol_Header]            = ImVec4(0.76f, 0.82f, 0.94f, 1.0f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.68f, 0.76f, 0.92f, 1.0f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.60f, 0.70f, 0.90f, 1.0f);
        colors[ImGuiCol_Button]            = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.72f, 0.78f, 0.90f, 1.0f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.60f, 0.70f, 0.88f, 1.0f);
        colors[ImGuiCol_Tab]              = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
        colors[ImGuiCol_TabHovered]       = ImVec4(0.72f, 0.78f, 0.92f, 1.0f);
        colors[ImGuiCol_TabSelected]      = ImVec4(0.80f, 0.84f, 0.94f, 1.0f);
        colors[ImGuiCol_Separator]         = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
        colors[ImGuiCol_TableHeaderBg]     = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
        colors[ImGuiCol_TableBorderLight]  = ImVec4(0.82f, 0.82f, 0.85f, 1.0f);
        colors[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.00f, 0.00f, 0.00f, 0.03f);
    }
}

}  // namespace magic
