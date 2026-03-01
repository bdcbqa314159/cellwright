#include "core/Column.hpp"
#include <cmath>

namespace magic {

void Column::ensure_row(int32_t row) {
    if (row >= static_cast<int32_t>(doubles_.size()))
        doubles_.resize(row + 1, 0.0);
}

CellValue Column::get(int32_t row) const {
    auto it = overrides_.find(row);
    if (it != overrides_.end()) return it->second;
    if (row < static_cast<int32_t>(doubles_.size()))
        return CellValue{doubles_[row]};
    return CellValue{};  // monostate = empty
}

void Column::set(int32_t row, const CellValue& val) {
    if (is_number(val)) {
        overrides_.erase(row);
        ensure_row(row);
        doubles_[row] = as_number(val);
    } else {
        ensure_row(row);
        doubles_[row] = 0.0;
        overrides_[row] = val;
    }
}

void Column::clear(int32_t row) {
    overrides_.erase(row);
    if (row < static_cast<int32_t>(doubles_.size()))
        doubles_[row] = 0.0;
}

int32_t Column::size() const {
    return static_cast<int32_t>(doubles_.size());
}

double Column::sum(int32_t from, int32_t to) const {
    double s = 0.0;
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    for (int32_t r = from; r < end; ++r) {
        if (overrides_.count(r) == 0)
            s += doubles_[r];
        else {
            auto& ov = overrides_.at(r);
            if (is_number(ov)) s += as_number(ov);
        }
    }
    return s;
}

}  // namespace magic
