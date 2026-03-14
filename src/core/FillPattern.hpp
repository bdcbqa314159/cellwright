#pragma once
#include "core/CellValue.hpp"
#include "core/CellAddress.hpp"
#include "core/Sheet.hpp"
#include <cmath>
#include <vector>

namespace magic {

// Detect and apply fill patterns for the fill handle drag.
// Currently supports:
//   - Numeric arithmetic sequence (e.g., 1,2,3 → 4,5,6 with step=1)
//   - Single value repeat (fallback — same as current behavior)

struct FillPattern {
    enum class Kind { Repeat, Arithmetic };
    Kind kind = Kind::Repeat;
    double start = 0.0;
    double step = 0.0;
};

// Detect a pattern by looking at cells along the fill axis leading up to
// the source cell. Examines up to `lookback` cells.
// axis: true = vertical (filling rows), false = horizontal (filling columns).
// Returns Arithmetic if a consistent numeric step is found, Repeat otherwise.
inline FillPattern detect_pattern(const Sheet& sheet, const CellAddress& src,
                                   bool vertical, int lookback = 3) {
    FillPattern result;
    result.kind = FillPattern::Kind::Repeat;

    // Collect numeric values leading up to (and including) the source cell
    std::vector<double> values;
    for (int i = lookback; i >= 0; --i) {
        CellAddress addr = vertical
            ? CellAddress{src.col, src.row - i}
            : CellAddress{src.col - i, src.row};
        if (addr.col < 0 || addr.row < 0) continue;
        CellValue val = sheet.get_value(addr);
        if (is_number(val))
            values.push_back(as_number(val));
        else
            values.clear();  // non-numeric breaks the sequence
    }

    if (values.size() >= 2) {
        // Check for constant step
        double step = values[1] - values[0];
        bool consistent = true;
        for (std::size_t i = 2; i < values.size(); ++i) {
            if (std::abs((values[i] - values[i - 1]) - step) > 1e-9) {
                consistent = false;
                break;
            }
        }
        if (consistent && std::abs(step) > 1e-12) {
            result.kind = FillPattern::Kind::Arithmetic;
            result.start = values.back();
            result.step = step;
        }
    }

    return result;
}

// Generate the fill value for the n-th cell after the source (1-based).
inline CellValue fill_value(const FillPattern& pattern, int n) {
    if (pattern.kind == FillPattern::Kind::Arithmetic)
        return CellValue{pattern.start + pattern.step * n};
    return {};  // Repeat mode — caller uses the source value
}

}  // namespace magic
