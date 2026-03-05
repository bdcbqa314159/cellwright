#include "core/Column.hpp"
#include "core/SimdOps.hpp"
#include <cmath>
#include <limits>

namespace magic {

static constexpr double EMPTY_SENTINEL = std::numeric_limits<double>::quiet_NaN();

void Column::ensure_row(int32_t row) {
    static constexpr int32_t MAX_ROW = 1048576;
    if (row < 0 || row > MAX_ROW) return;
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

bool Column::has_non_numeric_in_range(int32_t from, int32_t to) const {
    for (const auto& [row, _] : non_numeric_) {
        if (row >= from && row < to) return true;
    }
    return false;
}

double Column::sum(int32_t from, int32_t to) const {
    int32_t start = std::max(from, int32_t{0});
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    if (start >= end) return 0.0;

    // Fast path: no non-numeric entries in range — use SIMD
    if (!has_non_numeric_in_range(start, end))
        return simd_sum(doubles_.data() + start, static_cast<size_t>(end - start));

    // Slow path: mixed types
    double s = 0.0;
    for (int32_t r = start; r < end; ++r) {
        if (!non_numeric_.contains(r)) {
            double d = doubles_[r];
            if (!std::isnan(d)) s += d;
        } else {
            auto& ov = non_numeric_.at(r);
            if (is_number(ov)) s += as_number(ov);
        }
    }
    return s;
}

double Column::min(int32_t from, int32_t to) const {
    int32_t start = std::max(from, int32_t{0});
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    if (start >= end) return std::numeric_limits<double>::infinity();

    if (!has_non_numeric_in_range(start, end))
        return simd_min(doubles_.data() + start, static_cast<size_t>(end - start));

    double result = std::numeric_limits<double>::infinity();
    for (int32_t r = start; r < end; ++r) {
        double d;
        if (!non_numeric_.contains(r)) {
            d = doubles_[r];
        } else {
            auto& ov = non_numeric_.at(r);
            if (!is_number(ov)) continue;
            d = as_number(ov);
        }
        if (!std::isnan(d) && d < result) result = d;
    }
    return result;
}

double Column::max(int32_t from, int32_t to) const {
    int32_t start = std::max(from, int32_t{0});
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    if (start >= end) return -std::numeric_limits<double>::infinity();

    if (!has_non_numeric_in_range(start, end))
        return simd_max(doubles_.data() + start, static_cast<size_t>(end - start));

    double result = -std::numeric_limits<double>::infinity();
    for (int32_t r = start; r < end; ++r) {
        double d;
        if (!non_numeric_.contains(r)) {
            d = doubles_[r];
        } else {
            auto& ov = non_numeric_.at(r);
            if (!is_number(ov)) continue;
            d = as_number(ov);
        }
        if (!std::isnan(d) && d > result) result = d;
    }
    return result;
}

size_t Column::count_numeric(int32_t from, int32_t to) const {
    int32_t start = std::max(from, int32_t{0});
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    if (start >= end) return 0;

    if (!has_non_numeric_in_range(start, end))
        return simd_count_numeric(doubles_.data() + start, static_cast<size_t>(end - start));

    size_t result = 0;
    for (int32_t r = start; r < end; ++r) {
        if (!non_numeric_.contains(r)) {
            if (!std::isnan(doubles_[r])) ++result;
        } else {
            if (is_number(non_numeric_.at(r))) ++result;
        }
    }
    return result;
}

double Column::sum_of_squares(int32_t from, int32_t to, double mean) const {
    int32_t start = std::max(from, int32_t{0});
    int32_t end = std::min(to, static_cast<int32_t>(doubles_.size()));
    if (start >= end) return 0.0;

    if (!has_non_numeric_in_range(start, end))
        return simd_sum_of_squares(doubles_.data() + start, static_cast<size_t>(end - start), mean);

    double result = 0.0;
    for (int32_t r = start; r < end; ++r) {
        double d;
        if (!non_numeric_.contains(r)) {
            d = doubles_[r];
        } else {
            auto& ov = non_numeric_.at(r);
            if (!is_number(ov)) continue;
            d = as_number(ov);
        }
        if (!std::isnan(d)) {
            double diff = d - mean;
            result += diff * diff;
        }
    }
    return result;
}

}  // namespace magic
