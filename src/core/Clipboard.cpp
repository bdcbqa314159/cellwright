#include "core/Clipboard.hpp"
#include "core/Sheet.hpp"
#include "formula/Tokenizer.hpp"
#include <algorithm>
#include <sstream>

namespace magic {

void Clipboard::copy_single(const Sheet& sheet, const CellAddress& addr) {
    cells_.clear();
    origin_ = addr;

    ClipboardCell cc;
    cc.rel_col = 0;
    cc.rel_row = 0;
    cc.value = sheet.get_value(addr);
    if (sheet.has_formula(addr))
        cc.formula = sheet.get_formula(addr);
    cells_.push_back(std::move(cc));
}

void Clipboard::copy(const Sheet& sheet, const CellAddress& from, const CellAddress& to) {
    cells_.clear();
    origin_ = from;

    int32_t c1 = std::min(from.col, to.col);
    int32_t c2 = std::max(from.col, to.col);
    int32_t r1 = std::min(from.row, to.row);
    int32_t r2 = std::max(from.row, to.row);

    for (int32_t c = c1; c <= c2; ++c) {
        for (int32_t r = r1; r <= r2; ++r) {
            CellAddress addr{c, r};
            ClipboardCell cc;
            cc.rel_col = c - c1;
            cc.rel_row = r - r1;
            cc.value = sheet.get_value(addr);
            if (sheet.has_formula(addr))
                cc.formula = sheet.get_formula(addr);
            cells_.push_back(std::move(cc));
        }
    }
}

std::string Clipboard::adjust_references(const std::string& formula,
                                           int32_t dcol, int32_t drow) {
    if (dcol == 0 && drow == 0) return formula;

    try {
        auto tokens = Tokenizer::tokenize(formula);
        std::ostringstream out;

        for (const auto& tok : tokens) {
            if (tok.type == TokenType::CELLREF) {
                auto addr = CellAddress::from_a1(tok.text);
                if (addr) {
                    addr->col += dcol;
                    addr->row += drow;
                    if (addr->col >= 0 && addr->row >= 0) {
                        out << addr->to_a1();
                    } else {
                        out << tok.text;  // keep original if adjustment goes negative
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
        return formula;  // return unmodified on parse error
    }
}

std::vector<Clipboard::PasteEntry> Clipboard::paste_at(const CellAddress& dest) const {
    std::vector<PasteEntry> entries;
    entries.reserve(cells_.size());

    for (const auto& cc : cells_) {
        PasteEntry pe;
        pe.addr = {dest.col + cc.rel_col, dest.row + cc.rel_row};

        if (!cc.formula.empty()) {
            int32_t dcol = pe.addr.col - (origin_.col + cc.rel_col);
            int32_t drow = pe.addr.row - (origin_.row + cc.rel_row);
            pe.formula = adjust_references(cc.formula, dcol, drow);
        }

        pe.value = cc.value;
        entries.push_back(std::move(pe));
    }

    return entries;
}

}  // namespace magic
