#include "core/CellValue.hpp"
#include <cmath>
#include <sstream>

namespace magic {

std::string to_display_string(const CellValue& v) {
    if (is_empty(v)) return "";
    if (is_number(v)) {
        double d = as_number(v);
        if (d == static_cast<int64_t>(d))
            return std::to_string(static_cast<int64_t>(d));
        std::ostringstream oss;
        oss << d;
        return oss.str();
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
