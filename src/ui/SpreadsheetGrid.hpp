#pragma once
#include "core/CellAddress.hpp"
#include "ui/CellEditor.hpp"

namespace magic {

class Sheet;
class FormatMap;

struct GridState {
    CellAddress selected{0, 0};
    // Selection range (anchor + selected define the range)
    CellAddress sel_anchor{0, 0};
    bool has_range_selection = false;
    CellEditor editor;

    // Get the rectangular selection bounds
    CellAddress sel_min() const {
        if (!has_range_selection) return selected;
        return {std::min(sel_anchor.col, selected.col), std::min(sel_anchor.row, selected.row)};
    }
    CellAddress sel_max() const {
        if (!has_range_selection) return selected;
        return {std::max(sel_anchor.col, selected.col), std::max(sel_anchor.row, selected.row)};
    }
};

class SpreadsheetGrid {
public:
    // Render the spreadsheet grid. Returns true if a cell value was committed.
    bool render(Sheet& sheet, GridState& state, const FormatMap& formats);

private:
    static constexpr int VISIBLE_COLS = 26;
    static constexpr float COL_WIDTH = 80.0f;
    static constexpr float ROW_NUM_WIDTH = 50.0f;
};

}  // namespace magic
