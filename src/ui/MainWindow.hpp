#pragma once
#include "ui/SpreadsheetGrid.hpp"
#include "ui/FormulaBar.hpp"

namespace magic {

class Workbook;
class FunctionRegistry;

class MainWindow {
public:
    void render(Workbook& workbook, FunctionRegistry& registry);

    GridState& grid_state() { return grid_state_; }

private:
    SpreadsheetGrid grid_;
    FormulaBar formula_bar_;
    GridState grid_state_;
};

}  // namespace magic
