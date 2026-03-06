#pragma once
#include "core/CellAddress.hpp"
#include "core/CellValue.hpp"
#include "core/Column.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace magic {

class Sheet {
public:
    explicit Sheet(const std::string& name = "Sheet1", int32_t cols = 26, int32_t rows = 1000);

    const std::string& name() const { return name_; }
    void set_name(const std::string& n) { name_ = n; }

    int32_t col_count() const { return static_cast<int32_t>(columns_.size()); }
    int32_t row_count() const { return row_count_; }

    CellValue get_value(const CellAddress& addr) const;
    void set_value(const CellAddress& addr, const CellValue& val);

    // Formula storage
    bool has_formula(const CellAddress& addr) const;
    const std::string& get_formula(const CellAddress& addr) const;
    void set_formula(const CellAddress& addr, const std::string& formula);
    void clear_formula(const CellAddress& addr);

    // Dirty tracking for recalculation
    void mark_dirty(const CellAddress& addr);
    const std::unordered_set<CellAddress>& dirty_cells() const { return dirty_; }
    void clear_dirty();

    // Row/column insertion and deletion
    void insert_row(int32_t at);
    void delete_row(int32_t at);
    void insert_column(int32_t at);
    void delete_column(int32_t at);

    Column& column(int32_t col);
    const Column& column(int32_t col) const;

    uint64_t value_generation() const { return value_generation_; }

private:
    std::string name_;
    int32_t row_count_;
    std::vector<Column> columns_;
    std::unordered_map<CellAddress, std::string> formulas_;
    std::unordered_set<CellAddress> dirty_;
    uint64_t value_generation_ = 0;
    static const std::string empty_formula_;
};

}  // namespace magic
