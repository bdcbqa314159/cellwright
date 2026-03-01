#pragma once
#include "core/CellAddress.hpp"

namespace magic {

class Sheet;
class FunctionRegistry;

class FormulaBar {
public:
    // Render the formula bar. Returns true if the user committed a new value.
    bool render(Sheet& sheet, const CellAddress& selected, bool cell_editing = false);
    const char* buffer() const { return buf_; }
    bool is_editing() const { return editing_; }
    bool is_formula_mode() const { return editing_ && buf_[0] == '='; }

private:
    char buf_[1024] = {};
    CellAddress last_selected_{-1, -1};
    bool editing_ = false;
};

}  // namespace magic
