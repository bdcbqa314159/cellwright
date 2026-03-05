#include <gtest/gtest.h>
#include "core/DuckDBEngine.hpp"
#include "core/Sheet.hpp"

using namespace magic;

TEST(DuckDBEngine, BasicQuery) {
    DuckDBEngine engine;
    auto result = engine.query("SELECT 1 + 1 AS answer");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.col_count(), 1u);
    ASSERT_EQ(result.row_count(), 1u);
    ASSERT_TRUE(is_number(result.columns[0][0]));
    EXPECT_DOUBLE_EQ(as_number(result.columns[0][0]), 2.0);
}

TEST(DuckDBEngine, ImportAndQuery) {
    DuckDBEngine engine;
    Sheet sheet("test", 2, 100);

    sheet.set_value({0, 0}, CellValue{std::string("Alice")});
    sheet.set_value({1, 0}, CellValue{100.0});
    sheet.set_value({0, 1}, CellValue{std::string("Bob")});
    sheet.set_value({1, 1}, CellValue{200.0});

    engine.import_sheet(sheet, "data");
    auto result = engine.query("SELECT SUM(CAST(B AS DOUBLE)) AS total FROM data");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.row_count(), 1u);
    ASSERT_TRUE(is_number(result.columns[0][0]));
    EXPECT_DOUBLE_EQ(as_number(result.columns[0][0]), 300.0);
}

TEST(DuckDBEngine, InvalidSQL) {
    DuckDBEngine engine;
    auto result = engine.query("SELECT FROM WHERE");
    EXPECT_FALSE(result.ok());
    EXPECT_FALSE(result.error.empty());
}

TEST(DuckDBEngine, ExportToSheet) {
    DuckDBEngine engine;
    auto result = engine.query("SELECT 42 AS val, 'hello' AS text");
    ASSERT_TRUE(result.ok());

    Sheet sheet("export", 10, 100);
    DuckDBEngine::export_to_sheet(result, sheet);

    auto v0 = sheet.get_value({0, 0});
    ASSERT_TRUE(is_number(v0));
    EXPECT_DOUBLE_EQ(as_number(v0), 42.0);

    auto v1 = sheet.get_value({1, 0});
    ASSERT_TRUE(is_string(v1));
    EXPECT_EQ(as_string(v1), "hello");
}

TEST(DuckDBEngine, CachesImportGeneration) {
    DuckDBEngine engine;
    Sheet sheet("test", 2, 100);
    sheet.set_value({0, 0}, CellValue{1.0});

    // First import
    engine.import_sheet(sheet, "data");
    auto r1 = engine.query("SELECT COUNT(*) FROM data");
    ASSERT_TRUE(r1.ok());

    // Second import with same generation should be cached (no-op)
    engine.import_sheet(sheet, "data");
    auto r2 = engine.query("SELECT COUNT(*) FROM data");
    ASSERT_TRUE(r2.ok());
    EXPECT_DOUBLE_EQ(as_number(r1.columns[0][0]), as_number(r2.columns[0][0]));
}
