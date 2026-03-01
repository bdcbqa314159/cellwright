#pragma once

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
    void render_plot(Sheet& sheet);

    bool visible_ = false;
    ChartType chart_type_ = ChartType::Line;
    int x_col_ = -1;   // -1 = use row index
    int y_col_ = 0;
};

}  // namespace magic
