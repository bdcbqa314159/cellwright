#pragma once
#include "ui/SpreadsheetGrid.hpp"
#include "ui/FormulaBar.hpp"
#include "ui/ChartPanel.hpp"
#include "ui/StyleSetup.hpp"
#include <cstdint>

namespace magic {

struct AppState;

class MainWindow {
public:
    void render(AppState& state);

    GridState& grid_state() { return grid_state_; }

private:
    void render_menu_bar(AppState& state);
    void handle_keyboard(AppState& state);

    SpreadsheetGrid grid_;
    FormulaBar formula_bar_;
    GridState grid_state_;
    ChartPanel chart_panel_;

    Theme theme_ = Theme::Dark;
    float zoom_ = 1.0f;

    // Cached selection stats (recomputed only when selection or data changes)
    CellAddress cached_sel_min_{-1, -1};
    CellAddress cached_sel_max_{-1, -1};
    uint64_t cached_sel_gen_ = 0;
    double cached_sum_ = 0;
    int cached_count_ = 0;
    bool cached_has_range_ = false;

    // Cached plugin hash (avoid SHA-256 every frame while trust modal is open)
    std::string cached_plugin_path_;
    std::string cached_plugin_hash_;
};

}  // namespace magic
