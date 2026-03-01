#pragma once
#include "core/CellAddress.hpp"

namespace magic {

class Sheet;
class FunctionRegistry;

class FormulaBar {
public:
    // Render the formula bar. Returns true if the user committed a new value.
    bool render(Sheet& sheet, const CellAddress& selected);
};

}  // namespace magic
