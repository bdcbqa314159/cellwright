#include "ui/ChartPanel.hpp"
#include "core/Sheet.hpp"
#include "core/CellAddress.hpp"

#include <imgui.h>
#include <implot.h>

#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "util/stb_image_write.h"
#include <nfd.hpp>

namespace magic {

static constexpr int kColormapCount = 6;
static const char* kColormapNames[] = {"Deep", "Dark", "Pastel", "Viridis", "Plasma", "Spectral"};
static const ImPlotColormap kColormaps[] = {
    ImPlotColormap_Deep,
    ImPlotColormap_Dark,
    ImPlotColormap_Pastel,
    ImPlotColormap_Viridis,
    ImPlotColormap_Plasma,
    ImPlotColormap_Spectral,
};

void ChartPanel::render(Sheet& sheet) {
    if (!visible_) return;

    ImGui::Begin("Chart", &visible_);
    render_controls(sheet);
    render_style_controls();

    if (ImGui::Button("Export PNG...")) {
        export_countdown_ = 2;  // wait 2 frames for render to complete
    }
    if (export_status_timer_ > 0.0f) {
        ImGui::SameLine();
        ImGui::TextUnformatted(export_status_.c_str());
        export_status_timer_ -= ImGui::GetIO().DeltaTime;
    }

    ImGui::Separator();
    render_plot(sheet);

    // Deferred capture: countdown ensures the plot is fully rendered and
    // swapped to the front buffer before glReadPixels captures it.
    if (export_countdown_ > 0) {
        --export_countdown_;
        if (export_countdown_ == 0)
            export_png();
    }

    ImGui::End();
}

void ChartPanel::render_controls(Sheet& sheet) {
    // Chart type combo
    const char* type_labels[] = {"Line", "Bar", "Scatter", "Histogram"};
    int type_idx = static_cast<int>(chart_type_);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("Type", &type_idx, type_labels, 4)) {
        type_idx = std::clamp(type_idx, 0, 3);
        chart_type_ = static_cast<ChartType>(type_idx);
    }

    ImGui::SameLine();

    // X column combo (with "Index" option)
    int num_cols = sheet.col_count();
    {
        std::string x_preview = (x_col_ < 0) ? "Index" : CellAddress::col_to_letters(x_col_);
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("X", x_preview.c_str())) {
            if (ImGui::Selectable("Index", x_col_ < 0))
                x_col_ = -1;
            for (int c = 0; c < num_cols; ++c) {
                std::string label = CellAddress::col_to_letters(c);
                if (ImGui::Selectable(label.c_str(), x_col_ == c))
                    x_col_ = c;
            }
            ImGui::EndCombo();
        }
    }

    // Clamp x_col_ if columns were removed (#42)
    if (x_col_ >= num_cols) x_col_ = -1;

    // Y columns — row of checkboxes (truncate if columns were removed)
    if (static_cast<int>(y_col_selected_.size()) > num_cols)
        y_col_selected_.resize(static_cast<size_t>(num_cols));
    y_col_selected_.resize(static_cast<size_t>(num_cols), false);
    if (num_cols > 0 && std::none_of(y_col_selected_.begin(), y_col_selected_.end(), [](bool v){ return v; })) {
        y_col_selected_[0] = true;  // default: select first column
    }

    ImGui::Text("Y:");
    ImGui::SameLine();
    for (int c = 0; c < num_cols; ++c) {
        std::string label = CellAddress::col_to_letters(c) + "##ycol";
        ImGui::SameLine();
        bool selected = y_col_selected_[static_cast<size_t>(c)];
        if (ImGui::Checkbox(label.c_str(), &selected)) {
            y_col_selected_[static_cast<size_t>(c)] = selected;
        }
    }
}

void ChartPanel::render_style_controls() {
    if (!ImGui::CollapsingHeader("Style")) return;

    // Colormap dropdown
    ImGui::SetNextItemWidth(120);
    if (ImGui::Combo("Colormap", &colormap_idx_, kColormapNames, kColormapCount)) {
        // selection stored in colormap_idx_
    }

    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Chart title", title_, sizeof(title_));

    ImGui::Checkbox("Show markers", &show_markers_);
    ImGui::SameLine();
    ImGui::Checkbox("Shade under first series", &shade_under_);
    ImGui::SameLine();
    ImGui::Checkbox("Min/Max annotations", &show_annotations_);

    if (chart_type_ == ChartType::Bar)
        ImGui::Checkbox("Stacked bars", &stacked_);
    ImGui::Checkbox("Log scale (Y)", &log_scale_);
    ImGui::SameLine();
    ImGui::Checkbox("Grid lines", &show_grid_lines_);

    ImGui::SetNextItemWidth(120);
    ImGui::InputText("X label", x_label_, sizeof(x_label_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Y label", y_label_, sizeof(y_label_));
}

void ChartPanel::render_plot(Sheet& sheet) {
    colormap_idx_ = std::clamp(colormap_idx_, 0, kColormapCount - 1);
    ImPlot::PushColormap(kColormaps[colormap_idx_]);

    const char* plot_title = (title_[0] != '\0') ? title_ : "##chart";
    if (ImPlot::BeginPlot(plot_title, ImVec2(-1, -1))) {
        // Capture plot area for PNG export
        plot_min_ = ImPlot::GetPlotPos();
        plot_max_ = ImVec2(plot_min_.x + ImPlot::GetPlotSize().x,
                           plot_min_.y + ImPlot::GetPlotSize().y);
        ImPlotAxisFlags x_flags = ImPlotAxisFlags_AutoFit;
        ImPlotAxisFlags y_flags = ImPlotAxisFlags_AutoFit;
        if (!show_grid_lines_) {
            x_flags |= ImPlotAxisFlags_NoGridLines;
            y_flags |= ImPlotAxisFlags_NoGridLines;
        }
        ImPlot::SetupAxes(x_label_, y_label_, x_flags, y_flags);
        if (log_scale_)
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);

        ImPlotSpec spec;
        if (show_markers_) {
            spec.Marker = ImPlotMarker_Circle;
            spec.MarkerSize = 4;
        }

        bool is_first_series = true;

        // Stacked bar mode: use PlotBarGroups API
        if (chart_type_ == ChartType::Bar && stacked_) {
            // Collect selected Y column indices
            std::vector<int> y_cols;
            for (int c = 0; c < sheet.col_count(); ++c) {
                if (static_cast<size_t>(c) < y_col_selected_.size() && y_col_selected_[static_cast<size_t>(c)])
                    y_cols.push_back(c);
            }
            if (!y_cols.empty()) {
                int num_rows = sheet.row_count();
                int item_count = static_cast<int>(y_cols.size());
                // Build label array
                std::vector<std::string> label_strs;
                std::vector<const char*> labels;
                for (int c : y_cols) {
                    label_strs.push_back(CellAddress::col_to_letters(c));
                    labels.push_back(label_strs.back().c_str());
                }
                // Build flat row-major data: [series0_group0, series0_group1, ..., series1_group0, ...]
                std::vector<double> data(static_cast<size_t>(item_count * num_rows), 0.0);
                for (int si = 0; si < item_count; ++si) {
                    const auto& col_data = sheet.column(y_cols[si]).doubles();
                    for (int r = 0; r < num_rows && r < static_cast<int>(col_data.size()); ++r) {
                        double v = col_data[r];
                        data[static_cast<size_t>(si * num_rows + r)] = std::isnan(v) ? 0.0 : v;
                    }
                }
                ImPlotSpec bar_spec;
                bar_spec.Flags = ImPlotBarGroupsFlags_Stacked;
                ImPlot::PlotBarGroups(labels.data(), data.data(), item_count, num_rows, 0.67, 0, bar_spec);
            }
            // Skip per-series rendering below
            ImPlot::EndPlot();
            ImPlot::PopColormap();
            return;
        }

        for (int c = 0; c < sheet.col_count(); ++c) {
            if (static_cast<size_t>(c) >= y_col_selected_.size() || !y_col_selected_[static_cast<size_t>(c)])
                continue;

            const auto& y_doubles = sheet.column(c).doubles();

            // Build xs/ys, filtering out NaN entries
            std::vector<double> xs, ys;
            if (x_col_ >= 0 && x_col_ < sheet.col_count()) {
                const auto& x_doubles = sheet.column(x_col_).doubles();
                // Cast is safe: size is bounded by sheet dimensions (MAX_ROW)
                int n = static_cast<int>(std::min(x_doubles.size(), y_doubles.size()));
                for (int i = 0; i < n; ++i) {
                    if (!std::isnan(x_doubles[i]) && !std::isnan(y_doubles[i])) {
                        xs.push_back(x_doubles[i]);
                        ys.push_back(y_doubles[i]);
                    }
                }
            } else {
                // Cast is safe: size is bounded by sheet dimensions (MAX_ROW)
                for (int i = 0; i < static_cast<int>(y_doubles.size()); ++i) {
                    if (!std::isnan(y_doubles[i])) {
                        xs.push_back(static_cast<double>(i));
                        ys.push_back(y_doubles[i]);
                    }
                }
            }

            if (xs.empty()) continue;

            std::string label = CellAddress::col_to_letters(c);
            int count = static_cast<int>(xs.size());

            // Shade under first series
            if (shade_under_ && is_first_series) {
                ImPlot::PlotShaded(label.c_str(), xs.data(), ys.data(), count, 0.0);
            }

            switch (chart_type_) {
                case ChartType::Line:
                    ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), count, spec);
                    break;
                case ChartType::Bar:
                    ImPlot::PlotBars(label.c_str(), xs.data(), ys.data(), count, 0.67, spec);
                    break;
                case ChartType::Scatter:
                    ImPlot::PlotScatter(label.c_str(), xs.data(), ys.data(), count, spec);
                    break;
                case ChartType::Histogram:
                    ImPlot::PlotHistogram(label.c_str(), ys.data(), count);
                    break;
            }

            // Min/Max annotations for first series
            if (show_annotations_ && is_first_series) {
                auto min_it = std::min_element(ys.begin(), ys.end());
                auto max_it = std::max_element(ys.begin(), ys.end());
                int min_i = static_cast<int>(min_it - ys.begin());
                int max_i = static_cast<int>(max_it - ys.begin());

                char buf[64];
                std::snprintf(buf, sizeof(buf), "Min: %.2f", ys[min_i]);
                ImPlot::PlotText(buf, xs[min_i], ys[min_i], ImVec2(0, 10));

                std::snprintf(buf, sizeof(buf), "Max: %.2f", ys[max_i]);
                ImPlot::PlotText(buf, xs[max_i], ys[max_i], ImVec2(0, -10));
            }

            is_first_series = false;
        }

        // Hover tooltip
        if (ImPlot::IsPlotHovered()) {
            ImPlotPoint mouse = ImPlot::GetPlotMousePos();
            ImGui::BeginTooltip();
            ImGui::Text("X: %.3f  Y: %.3f", mouse.x, mouse.y);
            ImGui::EndTooltip();
        }

        ImPlot::EndPlot();
    }

    ImPlot::PopColormap();
}

void ChartPanel::export_png() {
    // Get the chart window rect (including controls, not just plot area)
    ImVec2 win_min = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    int x = static_cast<int>(win_min.x);
    int y_from_top = static_cast<int>(win_min.y);
    int w = static_cast<int>(win_size.x);
    int h = static_cast<int>(win_size.y);

    if (w <= 0 || h <= 0) return;

    // OpenGL framebuffer Y is bottom-up; ImGui Y is top-down
    int fb_w, fb_h;
    auto* vp = ImGui::GetMainViewport();
    fb_w = static_cast<int>(vp->Size.x);
    fb_h = static_cast<int>(vp->Size.y);

    // Account for DPI scaling (framebuffer may be larger than window coords)
    ImGuiIO& io = ImGui::GetIO();
    float scale_x = io.DisplayFramebufferScale.x;
    float scale_y = io.DisplayFramebufferScale.y;

    int px = static_cast<int>(x * scale_x);
    int py = static_cast<int>((fb_h / scale_y - y_from_top - h) * scale_y);
    int pw = static_cast<int>(w * scale_x);
    int ph = static_cast<int>(h * scale_y);

    if (pw <= 0 || ph <= 0) return;

    std::vector<unsigned char> pixels(static_cast<size_t>(pw * ph * 3));
    glReadPixels(px, py, pw, ph, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Flip vertically (OpenGL is bottom-up)
    int row_bytes = pw * 3;
    std::vector<unsigned char> row(row_bytes);
    for (int i = 0; i < ph / 2; ++i) {
        unsigned char* top = pixels.data() + i * row_bytes;
        unsigned char* bot = pixels.data() + (ph - 1 - i) * row_bytes;
        std::memcpy(row.data(), top, row_bytes);
        std::memcpy(top, bot, row_bytes);
        std::memcpy(bot, row.data(), row_bytes);
    }

    // Ask user for save path
    NFD::UniquePathN path;
    nfdfilteritem_t filter[] = {{"PNG Image", "png"}};
    auto result = NFD::SaveDialog(path, filter, 1, nullptr, "chart.png");
    if (result == NFD_OKAY && path) {
        int ok = stbi_write_png(path.get(), pw, ph, 3, pixels.data(), row_bytes);
        export_status_ = ok ? "PNG saved" : "Export failed";
        export_status_timer_ = 3.0f;
    }
}

}  // namespace magic
