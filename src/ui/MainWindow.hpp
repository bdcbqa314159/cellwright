#pragma once
#include "ui/SpreadsheetGrid.hpp"
#include "ui/FormulaBar.hpp"
#include "ui/FindBar.hpp"
#include "ui/ChartPanel.hpp"
#include "ui/SqlPanel.hpp"
#include "ui/StyleSetup.hpp"
#include <cstdint>

namespace magic {

struct AppState;

struct FileDialogState {
    char path_buf[512] = {};
    bool show_open = false;
    bool show_save = false;
    bool show_import_csv = false;
    bool show_export_csv = false;
};

class MainWindow {
public:
    void render(AppState& state);

    GridState& grid_state() { return grid_state_; }

    // Whether user interaction is active (drags, formula mode, etc.)
    bool is_interaction_active() const {
        return grid_state_.drag_mode != CellDragMode::None ||
               grid_state_.formula_dragging;
    }

    // Signal that the user wants to close but has unsaved changes
    void request_close() { wants_close_ = true; }
    bool should_quit() const { return should_quit_; }

private:
    void render_menu_bar(AppState& state);
    void handle_keyboard(AppState& state);

    SpreadsheetGrid grid_;
    FormulaBar formula_bar_;
    GridState grid_state_;
    ChartPanel chart_panel_;
    FindBar find_bar_;
    SqlPanel sql_panel_;
    FileDialogState file_dialog_;

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
    std::string trust_modal_path_;
    std::string trust_modal_hash_;

    bool show_dirty_new_modal_ = false;
    bool wants_close_ = false;
    bool should_quit_ = false;
};

}  // namespace magic
