#pragma once

#include <vector>

namespace magic {

class Sheet;

enum class ChartType { Line, Bar, Scatter, Histogram };

class ChartPanel {
public:
    void render(Sheet& sheet);

    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

private:
    void render_controls(Sheet& sheet);
    void render_style_controls();
    void render_plot(Sheet& sheet);

    bool visible_ = false;
    ChartType chart_type_ = ChartType::Line;
    int x_col_ = -1;   // -1 = use row index

    std::vector<bool> y_col_selected_;  // one bool per column (multi-select)

    bool show_markers_ = false;
    bool shade_under_ = false;
    bool show_annotations_ = false;
    bool log_scale_ = false;
    bool show_grid_lines_ = true;
    int colormap_idx_ = 0;
    char title_[128] = "";
    char x_label_[64] = "X";
    char y_label_[64] = "Y";
};

}  // namespace magic
