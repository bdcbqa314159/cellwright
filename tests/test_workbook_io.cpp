#include <gtest/gtest.h>
#include "io/WorkbookIO.hpp"

using namespace magic;

TEST(WorkbookIO, JsonRoundtrip) {
    Workbook wb;
    wb.active_sheet().set_value({0, 0}, CellValue{42.0});
    wb.active_sheet().set_value({1, 0}, CellValue{std::string("hello")});
    wb.active_sheet().set_formula({2, 0}, "A1+10");
    wb.active_sheet().set_value({2, 0}, CellValue{52.0});

    std::string json = WorkbookIO::to_json(wb);

    Workbook wb2;
    ASSERT_TRUE(WorkbookIO::from_json(json, wb2));

    EXPECT_DOUBLE_EQ(as_number(wb2.active_sheet().get_value({0, 0})), 42.0);
    EXPECT_EQ(as_string(wb2.active_sheet().get_value({1, 0})), "hello");
    EXPECT_TRUE(wb2.active_sheet().has_formula({2, 0}));
    EXPECT_EQ(wb2.active_sheet().get_formula({2, 0}), "A1+10");
}

TEST(WorkbookIO, MultiSheet) {
    Workbook wb;
    wb.add_sheet("Data");
    wb.sheet(1).set_value({0, 0}, CellValue{99.0});
    wb.set_active(1);

    std::string json = WorkbookIO::to_json(wb);

    Workbook wb2;
    ASSERT_TRUE(WorkbookIO::from_json(json, wb2));

    EXPECT_EQ(wb2.sheet_count(), 2);
    EXPECT_EQ(wb2.active_index(), 1);
    EXPECT_DOUBLE_EQ(as_number(wb2.sheet(1).get_value({0, 0})), 99.0);
}
