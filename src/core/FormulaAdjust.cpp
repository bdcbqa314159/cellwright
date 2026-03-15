#include "core/FormulaAdjust.hpp"
#include "core/CellAddress.hpp"
#include "formula/Tokenizer.hpp"
#include <sstream>

namespace magic {

// Parse $ markers from a CELLREF token (e.g., "$A$1", "A$1", "$A1", "A1").
// Returns whether column and row are absolute.
static void parse_abs_markers(const std::string& text, bool& col_abs, bool& row_abs) {
    col_abs = false;
    row_abs = false;
    size_t i = 0;
    if (i < text.size() && text[i] == '$') { col_abs = true; ++i; }
    while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i]))) ++i;
    if (i < text.size() && text[i] == '$') { row_abs = true; }
}

// Rebuild A1 reference preserving $ markers.
static std::string to_a1_with_abs(const CellAddress& addr, bool col_abs, bool row_abs) {
    std::string result;
    if (col_abs) result += '$';
    result += CellAddress::col_to_letters(addr.col);
    if (row_abs) result += '$';
    result += std::to_string(addr.row + 1);
    return result;
}

static std::string adjust_refs_impl(const std::string& formula, bool is_row, int32_t at, int32_t delta) {
    try {
        auto tokens = Tokenizer::tokenize(formula);
        std::ostringstream out;

        for (const auto& tok : tokens) {
            if (tok.type == TokenType::CELLREF) {
                bool col_abs, row_abs;
                parse_abs_markers(tok.text, col_abs, row_abs);
                // Skip adjustment if the affected axis is absolute
                bool is_abs = is_row ? row_abs : col_abs;
                auto addr = CellAddress::from_a1(tok.text);
                if (addr && !is_abs) {
                    int32_t& coord = is_row ? addr->row : addr->col;
                    if (delta < 0 && coord == at) {
                        out << "#REF!";
                    } else if (coord >= at) {
                        coord += delta;
                        if (coord >= 0)
                            out << to_a1_with_abs(*addr, col_abs, row_abs);
                        else
                            out << tok.text;
                    } else {
                        out << tok.text;
                    }
                } else {
                    out << tok.text;
                }
            } else if (tok.type == TokenType::END) {
                // skip
            } else {
                out << tok.text;
            }
        }
        return out.str();
    } catch (...) {
        return formula;
    }
}

std::string adjust_formula_for_insert_row(const std::string& formula, int32_t at) {
    return adjust_refs_impl(formula, true, at, 1);
}
std::string adjust_formula_for_delete_row(const std::string& formula, int32_t at) {
    return adjust_refs_impl(formula, true, at, -1);
}
std::string adjust_formula_for_insert_col(const std::string& formula, int32_t at) {
    return adjust_refs_impl(formula, false, at, 1);
}
std::string adjust_formula_for_delete_col(const std::string& formula, int32_t at) {
    return adjust_refs_impl(formula, false, at, -1);
}

}  // namespace magic
