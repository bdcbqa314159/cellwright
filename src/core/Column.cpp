#include "core/Column.hpp"
#include <cmath>
#include <limits>

namespace magic {

static constexpr double EMPTY_SENTINEL = std::numeric_limits<double>::quiet_NaN();

void Column::ensure_row(int32_t row) {
    if (row < 0) return;
    if (row >= static_cast<int32_t>(doubles_.size()))
        doubles_.resize(static_cast<size_t>(row) + 1, EMPTY_SENTINEL);
}

CellValue Column::get(int32_t row) const {
    if (row < 0) return CellValue{};
    auto it = non_numeric_.find(row);
    if (it != non_numeric_.end()) return it->second;
    if (row < static_cast<int32_t>(doubles_.size())) {
        double d = doubles_[row];
        if (std::isnan(d)) return CellValue{};  // empty
        return CellValue{d};
    }
    return CellValue{};  // monostate = empty
}

void Column::set(int32_t row, const CellValue& val) {
    if (row < 0) return;
    if (is_number(val)) {
        non_numeric_.erase(row);
        ensure_row(row);
        doubles_[row] = as_number(val);
    } else if (is_empty(val)) {
        non_numeric_.erase(row);
        if (row < static_cast<int32_t>(doubles_.size()))
            doubles_[row] = EMPTY_SENTINEL;
    } else {
        ensure_row(row);
        doubles_[row] = EMPTY_SENTINEL;
        non_numeric_[row] = val;
    }
}

void Column::clear(int32_t row) {
    if (row < 0) return;
    non_numeric_.erase(row);
    if (row < static_cast<int32_t>(doubles_.size()))
        doubles_[row] = EMPTY_SENTINEL;
}

int32_t Column::size() const {
    return static_cast<int32_t>(doubles_.size());
}

double Column::sum(int32_t from, int32_t to) const {
    double s = 0.0;
    int32_t start = std::max(from, int32_t{0});
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    for (int32_t r = start; r < end; ++r) {
        if (non_numeric_.count(r) == 0) {
            double d = doubles_[r];
            if (!std::isnan(d)) s += d;
        } else {
            auto& ov = non_numeric_.at(r);
            if (is_number(ov)) s += as_number(ov);
        }
    }
    return s;
}

}  // namespace magic
