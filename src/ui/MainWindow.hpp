#pragma once
#include "ui/SpreadsheetGrid.hpp"
#include "ui/FormulaBar.hpp"
#include "ui/FindBar.hpp"
#include "ui/ChartPanel.hpp"
#include "ui/SqlPanel.hpp"
#include "ui/StyleSetup.hpp"
#include "core/Clipboard.hpp"
#include <cstdint>

namespace magic {

struct AppState;

class MainWindow {
public:
    void render(AppState& state);

    GridState& grid_state() { return grid_state_; }

    // Whether user interaction is active (drags, formula mode, etc.)
    bool is_interaction_active() const {
        return grid_state_.drag.drag_mode != CellDragMode::None ||
               grid_state_.drag.formula_dragging;
    }

    // Signal that the user wants to close but has unsaved changes
    void request_close() { wants_close_ = true; }
    bool should_quit() const { return should_quit_; }

private:
    void render_menu_bar(AppState& state);
    void handle_keyboard(AppState& state);
    void handle_shortcuts(AppState& state);
    void handle_navigation(AppState& state);
    void update_marching_ants(const Clipboard& clip);
    void clear_marching_ants();
    void do_open(AppState& state);
    void do_save(AppState& state);
    void do_import_csv(AppState& state);
    void do_export_csv(AppState& state);

    // Shared action methods (called by both menu and keyboard handlers)
    void action_save(AppState& state);
    void action_undo(AppState& state);
    void action_redo(AppState& state);
    void action_copy(AppState& state);
    void action_cut(AppState& state);
    void action_paste(AppState& state);

    // Decomposed render sub-methods
    void dispatch_context_action(AppState& state, Sheet& sheet);
    void handle_drag_completion(AppState& state, Sheet& sheet);
    void render_sheet_tabs(AppState& state);
    void render_status_bar(AppState& state, Sheet& sheet);
    void render_modals(AppState& state);

    SpreadsheetGrid grid_;
    FormulaBar formula_bar_;
    GridState grid_state_;
    ChartPanel chart_panel_;
    FindBar find_bar_;
    SqlPanel sql_panel_;

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
    bool show_cond_format_ = false;  // conditional formatting editor window
    bool wants_close_ = false;
    bool should_quit_ = false;

    void render_cond_format_editor(AppState& state);
};

}  // namespace magic
