#include <gtest/gtest.h>
#include "core/ArrowBridge.hpp"
#include "core/Sheet.hpp"
#include "core/CellAddress.hpp"

#include <cmath>
#include <cstring>

using namespace magic;

// Helper: check validity bitmap bit
static bool bitmap_get(const uint8_t* bm, int64_t i) {
    return (bm[i / 8] >> (i % 8)) & 1;
}

TEST(ArrowBridge, ExportEmptySheet) {
    Sheet sheet("empty", 3, 0);

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(sheet, &schema, &array);

    EXPECT_STREQ(schema.format, "+s");
    EXPECT_EQ(schema.n_children, 3);
    EXPECT_EQ(array.length, 0);
    EXPECT_EQ(array.n_children, 3);

    for (int i = 0; i < 3; ++i) {
        EXPECT_STREQ(schema.children[i]->format, "g");
        EXPECT_EQ(array.children[i]->length, 0);
    }

    schema.release(&schema);
    array.release(&array);
}

TEST(ArrowBridge, ExportColumnNames) {
    Sheet sheet("test", 4, 5);

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(sheet, &schema, &array);

    EXPECT_STREQ(schema.children[0]->name, "A");
    EXPECT_STREQ(schema.children[1]->name, "B");
    EXPECT_STREQ(schema.children[2]->name, "C");
    EXPECT_STREQ(schema.children[3]->name, "D");

    schema.release(&schema);
    array.release(&array);
}

TEST(ArrowBridge, ExportNumericData) {
    Sheet sheet("nums", 2, 3);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({0, 1}, CellValue{2.0});
    sheet.set_value({0, 2}, CellValue{3.0});
    sheet.set_value({1, 0}, CellValue{10.0});
    sheet.set_value({1, 1}, CellValue{20.0});
    sheet.set_value({1, 2}, CellValue{30.0});

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(sheet, &schema, &array);

    EXPECT_EQ(array.length, 3);

    // Column A
    const auto* col_a = array.children[0];
    const auto* vals_a = static_cast<const double*>(col_a->buffers[1]);
    EXPECT_DOUBLE_EQ(vals_a[0], 1.0);
    EXPECT_DOUBLE_EQ(vals_a[1], 2.0);
    EXPECT_DOUBLE_EQ(vals_a[2], 3.0);
    EXPECT_EQ(col_a->null_count, 0);

    // Column B
    const auto* col_b = array.children[1];
    const auto* vals_b = static_cast<const double*>(col_b->buffers[1]);
    EXPECT_DOUBLE_EQ(vals_b[0], 10.0);
    EXPECT_DOUBLE_EQ(vals_b[1], 20.0);
    EXPECT_DOUBLE_EQ(vals_b[2], 30.0);
    EXPECT_EQ(col_b->null_count, 0);

    schema.release(&schema);
    array.release(&array);
}

TEST(ArrowBridge, ExportNullsForEmptyCells) {
    Sheet sheet("gaps", 1, 4);
    sheet.set_value({0, 0}, CellValue{1.0});
    // row 1 empty
    sheet.set_value({0, 2}, CellValue{3.0});
    // row 3 empty

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(sheet, &schema, &array);

    const auto* col = array.children[0];
    const auto* validity = static_cast<const uint8_t*>(col->buffers[0]);
    EXPECT_TRUE(bitmap_get(validity, 0));   // 1.0 → valid
    EXPECT_FALSE(bitmap_get(validity, 1));  // empty → null
    EXPECT_TRUE(bitmap_get(validity, 2));   // 3.0 → valid
    EXPECT_FALSE(bitmap_get(validity, 3));  // empty → null
    EXPECT_EQ(col->null_count, 2);

    schema.release(&schema);
    array.release(&array);
}

TEST(ArrowBridge, ZeroCopyPointer) {
    Sheet sheet("zc", 1, 3);
    sheet.set_value({0, 0}, CellValue{42.0});
    sheet.set_value({0, 1}, CellValue{43.0});
    sheet.set_value({0, 2}, CellValue{44.0});

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(sheet, &schema, &array);

    // The data pointer should point directly into the Column's storage
    const auto* exported = static_cast<const double*>(array.children[0]->buffers[1]);
    const double* internal = sheet.column(0).doubles().data();
    EXPECT_EQ(exported, internal);

    schema.release(&schema);
    array.release(&array);
}

TEST(ArrowBridge, RoundTrip) {
    Sheet original("src", 2, 3);
    original.set_value({0, 0}, CellValue{1.5});
    original.set_value({0, 1}, CellValue{2.5});
    original.set_value({0, 2}, CellValue{3.5});
    original.set_value({1, 0}, CellValue{10.0});
    // {1, 1} empty
    original.set_value({1, 2}, CellValue{30.0});

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(original, &schema, &array);

    Sheet imported = import_sheet_from_arrow(&schema, &array, "imported");

    EXPECT_EQ(imported.col_count(), 2);
    EXPECT_EQ(imported.row_count(), 3);
    EXPECT_DOUBLE_EQ(std::get<double>(imported.get_value({0, 0})), 1.5);
    EXPECT_DOUBLE_EQ(std::get<double>(imported.get_value({0, 1})), 2.5);
    EXPECT_DOUBLE_EQ(std::get<double>(imported.get_value({0, 2})), 3.5);
    EXPECT_DOUBLE_EQ(std::get<double>(imported.get_value({1, 0})), 10.0);
    EXPECT_TRUE(is_empty(imported.get_value({1, 1})));
    EXPECT_DOUBLE_EQ(std::get<double>(imported.get_value({1, 2})), 30.0);

    schema.release(&schema);
    array.release(&array);
}

TEST(ArrowBridge, ReleaseIdempotent) {
    Sheet sheet("rel", 1, 1);
    sheet.set_value({0, 0}, CellValue{1.0});

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(sheet, &schema, &array);

    // First release
    array.release(&array);
    EXPECT_EQ(array.release, nullptr);

    schema.release(&schema);
    EXPECT_EQ(schema.release, nullptr);
}

TEST(ArrowBridge, ImportSetsSheetName) {
    Sheet src("original", 1, 2);
    src.set_value({0, 0}, CellValue{1.0});
    src.set_value({0, 1}, CellValue{2.0});

    ArrowSchema schema{};
    ArrowArray array{};
    export_sheet_to_arrow(src, &schema, &array);

    Sheet imported = import_sheet_from_arrow(&schema, &array, "MySheet");
    EXPECT_EQ(imported.name(), "MySheet");

    schema.release(&schema);
    array.release(&array);
}
