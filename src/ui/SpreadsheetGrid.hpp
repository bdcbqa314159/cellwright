#pragma once
#include "core/CellAddress.hpp"
#include "ui/CellEditor.hpp"

namespace magic {

class Sheet;
class Workbook;
class FunctionRegistry;

struct GridState {
    CellAddress selected{0, 0};
    CellEditor editor;
};

class SpreadsheetGrid {
public:
    // Render the spreadsheet grid. Returns true if a cell value was committed.
    bool render(Sheet& sheet, GridState& state);

private:
    static constexpr int VISIBLE_COLS = 26;
    static constexpr float COL_WIDTH = 80.0f;
    static constexpr float ROW_NUM_WIDTH = 50.0f;
};

}  // namespace magic
