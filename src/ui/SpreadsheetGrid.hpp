#pragma once
#include "core/CellAddress.hpp"
#include "ui/CellEditor.hpp"
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace magic { class FunctionRegistry; class ConditionalFormatStore; class RowFilter; class FormulaBar; }

namespace magic {

class Sheet;
class FormatMap;

enum class CellZone { None, Interior, Edge, FillHandle };
enum class CellDragMode { None, Move, Select, Fill };
enum class ContextAction { None, Cut, Copy, Paste, Clear, InsertRow, InsertCol, DeleteRow, DeleteCol, SortAsc, SortDesc };

struct GridState {
    // ── Selection state ──────────────────────────────────────────────────────
    struct SelectionState {
        CellAddress selected_cell{0, 0};
        CellAddress sel_anchor{0, 0};
        bool has_range = false;

        CellAddress sel_min() const {
            if (!has_range) return selected_cell;
            return {std::min(sel_anchor.col, selected_cell.col), std::min(sel_anchor.row, selected_cell.row)};
        }
        CellAddress sel_max() const {
            if (!has_range) return selected_cell;
            return {std::max(sel_anchor.col, selected_cell.col), std::max(sel_anchor.row, selected_cell.row)};
        }
    };
    SelectionState selection;

    // ── Drag state ───────────────────────────────────────────────────────────
    struct DragState {
        CellDragMode drag_mode = CellDragMode::None;
        CellAddress drag_source{0, 0};
        CellAddress drag_target{0, 0};
        bool drag_completed = false;  // set true on mouse release after drag

        // Formula-mode drag (for click-drag range insertion)
        CellAddress formula_drag_origin{0, 0};
        CellAddress formula_drag_target{-1, -1};  // live hover target
        bool formula_dragging = false;
    };
    DragState drag;

    // ── Clipboard visual state ───────────────────────────────────────────────
    struct ClipboardVisual {
        bool show_marching_ants = false;
        CellAddress clip_min{0, 0};
        CellAddress clip_max{0, 0};
    };
    ClipboardVisual clipboard_visual;

    // ── Non-grouped state ────────────────────────────────────────────────────
    CellEditor editor;

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
    ImFont* mono_font = nullptr;  // monospace font for formula editing
    const ConditionalFormatStore* cond_format = nullptr;  // conditional formatting rules

    // Row filter (set by MainWindow each frame)
    RowFilter* row_filter = nullptr;

    // Formula bar (for click-to-insert references when editing in formula bar)
    FormulaBar* formula_bar = nullptr;

    // Freeze panes: number of frozen rows/cols beyond the header.
    // 0 = only the header row/row-number column are frozen (default).
    // e.g., freeze_rows=2 freezes the header + 2 data rows.
    int freeze_rows = 0;
    int freeze_cols = 0;

    // Find match highlighting (set by MainWindow each frame)
    const std::vector<CellAddress>* find_matches = nullptr;
    int find_match_index = -1;

    // ── Convenience accessors (backwards compatibility) ─────────────────────
    // These delegate to nested sub-structs for readability at call sites.
    CellAddress sel_min() const { return selection.sel_min(); }
    CellAddress sel_max() const { return selection.sel_max(); }
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
