#include "core/FormulaAdjust.hpp"
#include "core/CellAddress.hpp"
#include "formula/Tokenizer.hpp"
#include <sstream>

namespace magic {

static std::string adjust_refs_impl(const std::string& formula, bool is_row, int32_t at, int32_t delta) {
    try {
        auto tokens = Tokenizer::tokenize(formula);
        std::ostringstream out;

        for (const auto& tok : tokens) {
            if (tok.type == TokenType::CELLREF) {
                auto addr = CellAddress::from_a1(tok.text);
                if (addr) {
                    int32_t& coord = is_row ? addr->row : addr->col;
                    if (delta < 0 && coord == at) {
                        out << "#REF!";
                    } else if (coord >= at) {
                        coord += delta;
                        if (coord >= 0)
                            out << addr->to_a1();
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
