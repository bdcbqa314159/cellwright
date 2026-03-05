#pragma once
#include "core/CellAddress.hpp"
#include "core/CellValue.hpp"
#include <string>
#include <vector>

namespace magic {

class Sheet;

struct ClipboardCell {
    int32_t rel_col = 0;  // offset from top-left of selection
    int32_t rel_row = 0;
    CellValue value;
    std::string formula;  // empty if not a formula cell
};

class Clipboard {
public:
    void copy(const Sheet& sheet, const CellAddress& from, const CellAddress& to);
    void copy_single(const Sheet& sheet, const CellAddress& addr);

    bool has_data() const { return !cells_.empty(); }
    bool is_cut() const { return is_cut_; }
    void set_cut(bool v) { is_cut_ = v; }
    std::vector<CellAddress> source_cells() const;
    void clear() { cells_.clear(); is_cut_ = false; }

    // Get cells to paste at destination, adjusting relative references
    struct PasteEntry {
        CellAddress addr;
        CellValue value;
        std::string formula;  // already adjusted
    };
    std::vector<PasteEntry> paste_at(const CellAddress& dest) const;

    // Adjust cell references in a formula by a delta
    static std::string adjust_references(const std::string& formula,
                                          int32_t dcol, int32_t drow);

private:
    std::vector<ClipboardCell> cells_;
    CellAddress origin_;
    bool is_cut_ = false;
};

}  // namespace magic
