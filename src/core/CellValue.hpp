#pragma once
#include <string>
#include <variant>

namespace magic {

enum class CellError { REF, VALUE, DIV0, NAME, NA };

using CellValue = std::variant<std::monostate, double, std::string, bool, CellError>;

inline bool is_empty(const CellValue& v) { return std::holds_alternative<std::monostate>(v); }
inline bool is_number(const CellValue& v) { return std::holds_alternative<double>(v); }
inline bool is_string(const CellValue& v) { return std::holds_alternative<std::string>(v); }
inline bool is_bool(const CellValue& v) { return std::holds_alternative<bool>(v); }
inline bool is_error(const CellValue& v) { return std::holds_alternative<CellError>(v); }

inline double as_number(const CellValue& v) { return std::get<double>(v); }
inline const std::string& as_string(const CellValue& v) { return std::get<std::string>(v); }

// Convert any CellValue to a display string
std::string to_display_string(const CellValue& v);

// Try to coerce CellValue to double (empty→0, bool→0/1, string→parse, error→NaN)
double to_double(const CellValue& v);

}  // namespace magic
