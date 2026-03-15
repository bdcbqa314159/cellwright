#include "core/Clipboard.hpp"
#include "core/CellAddress.hpp"
#include "core/Sheet.hpp"
#include "formula/Tokenizer.hpp"
#include <cctype>
#include <sstream>

namespace magic {

// Parse $ markers from a CELLREF token.
static void parse_abs_markers(const std::string& text, bool& col_abs, bool& row_abs) {
    col_abs = false;
    row_abs = false;
    size_t i = 0;
    if (i < text.size() && text[i] == '$') { col_abs = true; ++i; }
    while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i]))) ++i;
    if (i < text.size() && text[i] == '$') { row_abs = true; }
}

static std::string to_a1_with_abs(const CellAddress& addr, bool col_abs, bool row_abs) {
    std::string result;
    if (col_abs) result += '$';
    result += CellAddress::col_to_letters(addr.col);
    if (row_abs) result += '$';
    result += std::to_string(addr.row + 1);
    return result;
}

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

    int32_t c1 = std::min(from.col, to.col);
    int32_t c2 = std::max(from.col, to.col);
    int32_t r1 = std::min(from.row, to.row);
    int32_t r2 = std::max(from.row, to.row);

    origin_ = {c1, r1};

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
                bool col_abs, row_abs;
                parse_abs_markers(tok.text, col_abs, row_abs);
                auto addr = CellAddress::from_a1(tok.text);
                if (addr) {
                    if (!col_abs) addr->col += dcol;
                    if (!row_abs) addr->row += drow;
                    if (addr->col >= 0 && addr->row >= 0) {
                        out << to_a1_with_abs(*addr, col_abs, row_abs);
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
        return formula;  // return unmodified on parse error
    }
}

std::vector<Clipboard::PasteEntry> Clipboard::paste_at(const CellAddress& dest) const {
    std::vector<PasteEntry> entries;
    entries.reserve(cells_.size());

    for (const auto& cc : cells_) {
        CellAddress pa{dest.col + cc.rel_col, dest.row + cc.rel_row};
        if (pa.col < 0 || pa.row < 0) continue;

        PasteEntry pe;
        pe.addr = pa;

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

std::vector<Clipboard::PasteEntry> Clipboard::paste_at_transposed(const CellAddress& dest) const {
    std::vector<PasteEntry> entries;
    entries.reserve(cells_.size());

    for (const auto& cc : cells_) {
        // Swap row/col offsets for transpose
        CellAddress pa{dest.col + cc.rel_row, dest.row + cc.rel_col};
        if (pa.col < 0 || pa.row < 0) continue;

        PasteEntry pe;
        pe.addr = pa;

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

std::vector<CellAddress> Clipboard::source_cells() const {
    std::vector<CellAddress> result;
    result.reserve(cells_.size());
    for (const auto& c : cells_) {
        result.push_back({origin_.col + c.rel_col, origin_.row + c.rel_row});
    }
    return result;
}

}  // namespace magic
