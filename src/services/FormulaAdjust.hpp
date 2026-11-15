#pragma once
#include <string>
#include <cstdint>

namespace magic {

// Adjust cell references in a formula for row/column insert/delete.
// Returns adjusted formula text, or embeds "#REF!" for deleted references.
std::string adjust_formula_for_insert_row(const std::string& formula, int32_t at);
std::string adjust_formula_for_delete_row(const std::string& formula, int32_t at);
std::string adjust_formula_for_insert_col(const std::string& formula, int32_t at);
std::string adjust_formula_for_delete_col(const std::string& formula, int32_t at);

}  // namespace magic
