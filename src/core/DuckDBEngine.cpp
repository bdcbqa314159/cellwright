#include "core/DuckDBEngine.hpp"
#include "core/Sheet.hpp"
#include "core/CellAddress.hpp"
#include "duckdb.hpp"
#include <unordered_map>

namespace magic {

static std::string escape_sql_identifier(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (c == '"') result += "\"\"";
        else result += c;
    }
    return result;
}

struct DuckDBEngine::Impl {
    duckdb::DuckDB db{nullptr};  // in-memory database
    duckdb::Connection conn{db};
    std::unordered_map<std::string, uint64_t> imported_generations_;
};

DuckDBEngine::DuckDBEngine() = default;
DuckDBEngine::~DuckDBEngine() = default;

void DuckDBEngine::ensure_init() {
    if (!impl_) impl_ = std::make_unique<Impl>();
}

void DuckDBEngine::import_sheet(const Sheet& sheet, const std::string& table_name) {
    ensure_init();
    uint64_t gen = sheet.value_generation();
    auto it = impl_->imported_generations_.find(table_name);
    if (it != impl_->imported_generations_.end() && it->second == gen)
        return;  // already up to date

    // Drop existing table
    std::string escaped = escape_sql_identifier(table_name);
    impl_->conn.Query("DROP TABLE IF EXISTS \"" + escaped + "\"");

    // Create table with columns A, B, C, ...
    int32_t ncols = sheet.col_count();
    int32_t nrows = sheet.row_count();

    std::string create_sql = "CREATE TABLE \"" + escaped + "\" (";
    for (int32_t c = 0; c < ncols; ++c) {
        if (c > 0) create_sql += ", ";
        create_sql += "\"" + CellAddress::col_to_letters(c) + "\" VARCHAR";
    }
    create_sql += ")";
    auto create_result = impl_->conn.Query(create_sql);
    if (create_result->HasError()) return;

    // Bulk insert using Appender
    duckdb::Appender appender(impl_->conn, table_name);
    for (int32_t r = 0; r < nrows; ++r) {
        bool has_data = false;
        for (int32_t c = 0; c < ncols; ++c) {
            if (!is_empty(sheet.get_value({c, r}))) {
                has_data = true;
                break;
            }
        }
        if (!has_data) continue;

        appender.BeginRow();
        for (int32_t c = 0; c < ncols; ++c) {
            CellValue val = sheet.get_value({c, r});
            if (is_number(val))
                appender.Append(duckdb::Value(std::to_string(as_number(val))));
            else if (is_string(val))
                appender.Append(duckdb::Value(as_string(val)));
            else if (is_bool(val))
                appender.Append(duckdb::Value(std::get<bool>(val) ? "TRUE" : "FALSE"));
            else
                appender.Append(duckdb::Value());
        }
        appender.EndRow();
    }
    appender.Close();

    impl_->imported_generations_[table_name] = gen;
}

QueryResult DuckDBEngine::query(const std::string& sql) {
    ensure_init();
    QueryResult qr;
    try {
        auto result = impl_->conn.Query(sql);
        if (result->HasError()) {
            qr.error = result->GetError();
            return qr;
        }

        size_t ncols = result->ColumnCount();
        qr.column_names.resize(ncols);
        qr.columns.resize(ncols);

        for (size_t c = 0; c < ncols; ++c)
            qr.column_names[c] = result->ColumnName(c);

        // Materialize all chunks
        while (true) {
            auto chunk = result->Fetch();
            if (!chunk || chunk->size() == 0) break;

            for (size_t c = 0; c < ncols; ++c) {
                auto& col_data = qr.columns[c];
                auto& vec = chunk->data[c];
                for (size_t r = 0; r < chunk->size(); ++r) {
                    auto val = vec.GetValue(r);
                    if (val.IsNull()) {
                        col_data.push_back(CellValue{});
                    } else {
                        // Try numeric conversion first
                        try {
                            double d = val.GetValue<double>();
                            col_data.push_back(CellValue{d});
                        } catch (const std::exception&) {
                            col_data.push_back(CellValue{val.ToString()});
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        qr.error = e.what();
    }
    return qr;
}

void DuckDBEngine::export_to_sheet(const QueryResult& result, Sheet& sheet) {
    for (size_t c = 0; c < result.col_count(); ++c) {
        for (size_t r = 0; r < result.row_count(); ++r) {
            CellAddress addr{static_cast<int32_t>(c), static_cast<int32_t>(r)};
            sheet.set_value(addr, result.columns[c][r]);
        }
    }
}

}  // namespace magic
