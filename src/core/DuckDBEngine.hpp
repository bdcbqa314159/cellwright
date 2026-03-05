#pragma once
#include "core/CellValue.hpp"
#include <memory>
#include <string>
#include <vector>

namespace duckdb {
class DuckDB;
class Connection;
}

namespace magic {

class Sheet;

struct QueryResult {
    std::vector<std::string> column_names;
    std::vector<std::vector<CellValue>> columns;  // column-major
    std::string error;

    bool ok() const { return error.empty(); }
    size_t row_count() const { return columns.empty() ? 0 : columns[0].size(); }
    size_t col_count() const { return columns.size(); }
};

class DuckDBEngine {
public:
    DuckDBEngine();
    ~DuckDBEngine();

    // Import a sheet as a DuckDB table. Skips re-import if generation unchanged.
    void import_sheet(const Sheet& sheet, const std::string& table_name);

    // Execute a SQL query and return results
    QueryResult query(const std::string& sql);

    // Write query results into a sheet starting at A1
    static void export_to_sheet(const QueryResult& result, Sheet& sheet);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace magic
