#include "core/Sheet.hpp"

namespace magic {

const std::string Sheet::empty_formula_;

Sheet::Sheet(const std::string& name, int32_t cols, int32_t rows)
    : name_(name), row_count_(rows), columns_(cols) {}

CellValue Sheet::get_value(const CellAddress& addr) const {
    if (addr.col < 0 || addr.row < 0 || addr.col >= col_count()) return CellValue{CellError::REF};
    return columns_[addr.col].get(addr.row);
}

void Sheet::set_value(const CellAddress& addr, const CellValue& val) {
    if (addr.col < 0 || addr.row < 0) return;
    while (addr.col >= col_count()) columns_.emplace_back();
    columns_[addr.col].set(addr.row, val);
    if (addr.row >= row_count_) row_count_ = addr.row + 1;
    ++value_generation_;
    mark_dirty(addr);
}

bool Sheet::has_formula(const CellAddress& addr) const {
    return formulas_.count(addr) > 0;
}

const std::string& Sheet::get_formula(const CellAddress& addr) const {
    auto it = formulas_.find(addr);
    if (it != formulas_.end()) return it->second;
    return empty_formula_;
}

void Sheet::set_formula(const CellAddress& addr, const std::string& formula) {
    formulas_[addr] = formula;
    mark_dirty(addr);
    ++value_generation_;
}

void Sheet::clear_formula(const CellAddress& addr) {
    formulas_.erase(addr);
}

void Sheet::mark_dirty(const CellAddress& addr) {
    dirty_.insert(addr);
}

void Sheet::clear_dirty() {
    dirty_.clear();
}

Column& Sheet::column(int32_t col) {
    if (col < 0) { static Column empty; return empty; }
    while (col >= col_count()) columns_.emplace_back();
    return columns_[col];
}

const Column& Sheet::column(int32_t col) const {
    if (col < 0 || col >= col_count()) { static const Column empty; return empty; }
    return columns_[col];
}

}  // namespace magic
