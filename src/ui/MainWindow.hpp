#pragma once
#include "ui/SpreadsheetGrid.hpp"
#include "ui/FormulaBar.hpp"

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
};

}  // namespace magic
