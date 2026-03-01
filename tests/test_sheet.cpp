#include <gtest/gtest.h>
#include "core/Sheet.hpp"

using namespace magic;

TEST(Sheet, SetAndGetValue) {
    Sheet sheet("Test");
    CellAddress addr{0, 0};
    sheet.set_value(addr, CellValue{42.0});

    auto val = sheet.get_value(addr);
    ASSERT_TRUE(is_number(val));
    EXPECT_DOUBLE_EQ(as_number(val), 42.0);
}

TEST(Sheet, Formula) {
    Sheet sheet("Test");
    CellAddress addr{0, 0};
    sheet.set_formula(addr, "A2+B2");

    EXPECT_TRUE(sheet.has_formula(addr));
    EXPECT_EQ(sheet.get_formula(addr), "A2+B2");
}

TEST(Sheet, DirtyTracking) {
    Sheet sheet("Test");
    CellAddress addr{0, 0};
    sheet.set_formula(addr, "1+1");

    EXPECT_EQ(sheet.dirty_cells().size(), 1u);
    EXPECT_TRUE(sheet.dirty_cells().count(addr) > 0);

    sheet.clear_dirty();
    EXPECT_TRUE(sheet.dirty_cells().empty());
}

TEST(Sheet, OutOfBounds) {
    Sheet sheet("Test");
    auto val = sheet.get_value({100, 0});
    EXPECT_TRUE(is_error(val));
}
