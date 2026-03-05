#pragma once
#include "core/CellValue.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace magic {

// Columnar storage: contiguous vector<double> for the fast path,
// sparse overrides for non-numeric cells.
class Column {
public:
    CellValue get(int32_t row) const;
    void set(int32_t row, const CellValue& val);
    void clear(int32_t row);
    int32_t size() const;

    // Fast-path aggregates over contiguous doubles (SIMD-accelerated when possible).
    // All use half-open interval [from, to) — i.e. 'from' inclusive, 'to' exclusive.
    [[nodiscard]] double sum(int32_t from, int32_t to) const;
    [[nodiscard]] double min(int32_t from, int32_t to) const;
    [[nodiscard]] double max(int32_t from, int32_t to) const;
    [[nodiscard]] size_t count_numeric(int32_t from, int32_t to) const;
    [[nodiscard]] double sum_of_squares(int32_t from, int32_t to, double mean) const;

    const std::vector<double>& doubles() const { return doubles_; }

private:
    void ensure_row(int32_t row);
    bool has_non_numeric_in_range(int32_t from, int32_t to) const;

    std::vector<double> doubles_;                          // dense numeric
    std::unordered_map<int32_t, CellValue> non_numeric_;   // sparse non-numeric
};

}  // namespace magic
