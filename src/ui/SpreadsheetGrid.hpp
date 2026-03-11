#pragma once
#include "core/CellAddress.hpp"
#include "ui/CellEditor.hpp"
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace magic { class FunctionRegistry; }

namespace magic {

class Sheet;
class FormatMap;

enum class CellZone { None, Interior, Edge, FillHandle };
enum class CellDragMode { None, Move, Select, Fill };
enum class ContextAction { None, Cut, Copy, Paste, Clear, InsertRow, InsertCol, DeleteRow, DeleteCol, SortAsc, SortDesc };

struct GridState {
    CellAddress selected{0, 0};
    // Selection range (anchor + selected define the range)
    CellAddress sel_anchor{0, 0};
    bool has_range_selection = false;
    CellEditor editor;

    // Formula-mode drag state (for click-drag range insertion)
    CellAddress formula_drag_origin{0, 0};
    CellAddress formula_drag_target{-1, -1};  // live hover target (for visual feedback)
    bool formula_dragging = false;

    // Formula bar buffer pointer (set by MainWindow each frame when formula bar is active)
    const char* formula_bar_buf = nullptr;

    // Theme flag (set by MainWindow each frame so overlay colors can adapt)
    bool dark_theme = true;

    // Cached reference highlight map (recomputed only when formula buffer changes)
    std::string cached_ref_buffer;
    std::unordered_map<CellAddress, int> cached_ref_colors;

    // Screen rect of the selected cell (updated each frame)
    ImVec2 selected_rect_min{0, 0};
    ImVec2 selected_rect_max{0, 0};

    // Context menu action (set by grid, consumed by MainWindow)
    ContextAction context_action = ContextAction::None;
    int32_t context_col = -1;  // column for sort actions

    // Function registry for autocomplete (set by MainWindow each frame)
    const FunctionRegistry* registry = nullptr;

    // Find match highlighting (set by MainWindow each frame)
    const std::vector<CellAddress>* find_matches = nullptr;
    int find_match_index = -1;

    // Clipboard source range (for marching ants animation)
    bool show_marching_ants = false;
    CellAddress clip_min{0, 0};
    CellAddress clip_max{0, 0};

    // Cell interaction drag state
    CellDragMode drag_mode = CellDragMode::None;
    CellAddress drag_source{0, 0};
    CellAddress drag_target{0, 0};
    bool drag_completed = false;  // set true on mouse release after drag

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
    static constexpr float DEFAULT_COL_WIDTH = 80.0f;
    static constexpr float ROW_NUM_WIDTH = 50.0f;

    std::vector<float> col_widths_;  // persisted column widths
};

}  // namespace magic
