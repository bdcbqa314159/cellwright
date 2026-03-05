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

    // Fast-path aggregate over contiguous doubles
    double sum(int32_t from, int32_t to) const;

    const std::vector<double>& doubles() const { return doubles_; }

private:
    void ensure_row(int32_t row);

    std::vector<double> doubles_;                          // dense numeric
    std::unordered_map<int32_t, CellValue> non_numeric_;   // sparse non-numeric
};

}  // namespace magic
