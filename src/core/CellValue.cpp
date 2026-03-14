#include "core/CellValue.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace magic {

std::string to_display_string(const CellValue& v) {
    if (is_empty(v)) return "";
    if (is_number(v)) {
        double d = as_number(v);
        // 2^53 is the largest integer exactly representable in double.
        // The isfinite() guard above ensures d is not NaN or Inf, which
        // protects the int64_t cast below from undefined behavior.
        constexpr double max_safe_int = 9007199254740992.0;
        if (std::isfinite(d) && std::abs(d) <= max_safe_int &&
            d == static_cast<int64_t>(d))
            return std::to_string(static_cast<int64_t>(d));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", d);
        return buf;
    }
    if (is_string(v)) return as_string(v);
    if (is_bool(v)) return std::get<bool>(v) ? "true" : "false";
    if (is_error(v)) {
        switch (std::get<CellError>(v)) {
            case CellError::REF:   return "#REF!";
            case CellError::VALUE: return "#VALUE!";
            case CellError::DIV0:  return "#DIV/0!";
            case CellError::NAME:  return "#NAME?";
            case CellError::NA:    return "#N/A";
        }
    }
    return "";
}

double to_double(const CellValue& v) {
    if (is_empty(v)) return 0.0;
    if (is_number(v)) return as_number(v);
    if (is_bool(v)) return std::get<bool>(v) ? 1.0 : 0.0;
    if (is_string(v)) {
        try { return std::stod(as_string(v)); }
        catch (...) { return std::nan(""); }
    }
    return std::nan("");
}

}  // namespace magic
