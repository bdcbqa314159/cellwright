#pragma once

namespace magic {

// A rectangle in OpenGL framebuffer pixels, origin bottom-left.
struct PixelRect {
    int x;
    int y;
    int w;
    int h;
};

// Convert a window rectangle expressed in ImGui's logical coordinates (origin
// top-left) into OpenGL framebuffer pixels (origin bottom-left).
//
// `viewport_h_logical` is ImGui::GetMainViewport()->Size.y, which is in LOGICAL
// units: the GLFW backend sets io.DisplaySize from the window size and
// io.DisplayFramebufferScale from framebuffer/window, so the viewport is never
// already in pixels. Scaling it is therefore the only conversion needed — the
// previous implementation also divided it by scale_y, which cancelled the
// conversion and offset the capture by a full logical screen height on any
// display with scale != 1.
//
// Pure and header-only on purpose: ChartPanel.cpp is linked only into the
// application (it needs ImGui/ImPlot/GL), so this is the only way the geometry
// can carry a test.
constexpr PixelRect framebuffer_rect(float win_x, float win_y_from_top,
                                     float win_w, float win_h,
                                     float viewport_h_logical,
                                     float scale_x, float scale_y) {
    const float y_from_bottom = viewport_h_logical - win_y_from_top - win_h;
    return PixelRect{
        static_cast<int>(win_x * scale_x),
        static_cast<int>(y_from_bottom * scale_y),
        static_cast<int>(win_w * scale_x),
        static_cast<int>(win_h * scale_y),
    };
}

}  // namespace magic
