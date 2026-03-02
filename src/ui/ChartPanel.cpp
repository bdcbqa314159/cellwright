#include "ui/ChartPanel.hpp"
#include "core/Sheet.hpp"
#include "core/CellAddress.hpp"

#include <imgui.h>
#include <implot.h>

#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

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
    ImGui::Separator();
    render_plot(sheet);
    ImGui::End();
}

void ChartPanel::render_controls(Sheet& sheet) {
    // Chart type combo
    const char* type_labels[] = {"Line", "Bar", "Scatter", "Histogram"};
    int type_idx = static_cast<int>(chart_type_);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("Type", &type_idx, type_labels, 4)) {
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

    // Y columns — row of checkboxes
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

    ImGui::Checkbox("Show markers", &show_markers_);
    ImGui::SameLine();
    ImGui::Checkbox("Shade under first series", &shade_under_);
    ImGui::SameLine();
    ImGui::Checkbox("Min/Max annotations", &show_annotations_);

    ImGui::SetNextItemWidth(120);
    ImGui::InputText("X label", x_label_, sizeof(x_label_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Y label", y_label_, sizeof(y_label_));
}

void ChartPanel::render_plot(Sheet& sheet) {
    ImPlot::PushColormap(kColormaps[colormap_idx_]);

    if (ImPlot::BeginPlot("##chart", ImVec2(-1, -1))) {
        ImPlot::SetupAxes(x_label_, y_label_, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);

        ImPlotSpec spec;
        if (show_markers_) {
            spec.Marker = ImPlotMarker_Circle;
            spec.MarkerSize = 4;
        }

        bool is_first_series = true;

        for (int c = 0; c < sheet.col_count(); ++c) {
            if (static_cast<size_t>(c) >= y_col_selected_.size() || !y_col_selected_[static_cast<size_t>(c)])
                continue;

            const auto& y_doubles = sheet.column(c).doubles();

            // Build xs/ys, filtering out NaN entries
            std::vector<double> xs, ys;
            if (x_col_ >= 0 && x_col_ < sheet.col_count()) {
                const auto& x_doubles = sheet.column(x_col_).doubles();
                int n = static_cast<int>(std::min(x_doubles.size(), y_doubles.size()));
                for (int i = 0; i < n; ++i) {
                    if (!std::isnan(x_doubles[i]) && !std::isnan(y_doubles[i])) {
                        xs.push_back(x_doubles[i]);
                        ys.push_back(y_doubles[i]);
                    }
                }
            } else {
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

}  // namespace magic
