#include "ui/ChartPanel.hpp"
#include "core/Sheet.hpp"
#include "core/CellAddress.hpp"

#include <imgui.h>
#include <implot.h>

#include <vector>
#include <cmath>
#include <string>

namespace magic {

void ChartPanel::render(Sheet& sheet) {
    if (!visible_) return;

    ImGui::Begin("Chart", &visible_);
    render_controls(sheet);
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

    ImGui::SameLine();

    // Y column combo
    {
        std::string y_preview = CellAddress::col_to_letters(y_col_);
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Y", y_preview.c_str())) {
            for (int c = 0; c < num_cols; ++c) {
                std::string label = CellAddress::col_to_letters(c);
                if (ImGui::Selectable(label.c_str(), y_col_ == c))
                    y_col_ = c;
            }
            ImGui::EndCombo();
        }
    }
}

void ChartPanel::render_plot(Sheet& sheet) {
    // Clamp y_col_ to valid range
    if (y_col_ >= sheet.col_count()) y_col_ = 0;

    const auto& y_doubles = sheet.column(y_col_).doubles();

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
        // Use row index as X
        for (int i = 0; i < static_cast<int>(y_doubles.size()); ++i) {
            if (!std::isnan(y_doubles[i])) {
                xs.push_back(static_cast<double>(i));
                ys.push_back(y_doubles[i]);
            }
        }
    }

    if (ImPlot::BeginPlot("##chart", ImVec2(-1, -1))) {
        if (!xs.empty()) {
            std::string label = CellAddress::col_to_letters(y_col_);
            switch (chart_type_) {
                case ChartType::Line:
                    ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
                    break;
                case ChartType::Bar:
                    ImPlot::PlotBars(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()), 0.67);
                    break;
                case ChartType::Scatter:
                    ImPlot::PlotScatter(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
                    break;
                case ChartType::Histogram:
                    ImPlot::PlotHistogram(label.c_str(), ys.data(), static_cast<int>(ys.size()));
                    break;
            }
        }
        ImPlot::EndPlot();
    }
}

}  // namespace magic
