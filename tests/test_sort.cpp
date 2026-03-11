#include <gtest/gtest.h>
#include "core/Sheet.hpp"
#include "core/Command.hpp"

using namespace magic;

class SortTest : public ::testing::Test {
protected:
    Sheet sheet{"Test", 3, 100};
};

TEST_F(SortTest, NumericAscending) {
    sheet.set_value({0, 0}, 30.0);
    sheet.set_value({0, 1}, 10.0);
    sheet.set_value({0, 2}, 20.0);

    SortColumnCommand cmd(0, true, sheet);
    cmd.execute(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 10.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 20.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 2})), 30.0);
}

TEST_F(SortTest, NumericDescending) {
    sheet.set_value({0, 0}, 10.0);
    sheet.set_value({0, 1}, 30.0);
    sheet.set_value({0, 2}, 20.0);

    SortColumnCommand cmd(0, false, sheet);
    cmd.execute(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 30.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 20.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 2})), 10.0);
}

TEST_F(SortTest, StringCaseInsensitive) {
    sheet.set_value({0, 0}, CellValue{std::string("banana")});
    sheet.set_value({0, 1}, CellValue{std::string("Apple")});
    sheet.set_value({0, 2}, CellValue{std::string("cherry")});

    SortColumnCommand cmd(0, true, sheet);
    cmd.execute(sheet);

    EXPECT_EQ(std::get<std::string>(sheet.get_value({0, 0})), "Apple");
    EXPECT_EQ(std::get<std::string>(sheet.get_value({0, 1})), "banana");
    EXPECT_EQ(std::get<std::string>(sheet.get_value({0, 2})), "cherry");
}

TEST_F(SortTest, EmptyCellsGoToBottom) {
    sheet.set_value({0, 0}, CellValue{});  // empty
    sheet.set_value({0, 1}, 10.0);
    sheet.set_value({0, 2}, 5.0);

    SortColumnCommand cmd(0, true, sheet);
    cmd.execute(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 5.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 10.0);
    EXPECT_TRUE(is_empty(sheet.get_value({0, 2})));
}

TEST_F(SortTest, MultiColumnRowsStayTogether) {
    // Col A: sort key, Col B: associated data
    sheet.set_value({0, 0}, 30.0);
    sheet.set_value({1, 0}, CellValue{std::string("thirty")});
    sheet.set_value({0, 1}, 10.0);
    sheet.set_value({1, 1}, CellValue{std::string("ten")});
    sheet.set_value({0, 2}, 20.0);
    sheet.set_value({1, 2}, CellValue{std::string("twenty")});

    SortColumnCommand cmd(0, true, sheet);
    cmd.execute(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 10.0);
    EXPECT_EQ(std::get<std::string>(sheet.get_value({1, 0})), "ten");
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 20.0);
    EXPECT_EQ(std::get<std::string>(sheet.get_value({1, 1})), "twenty");
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 2})), 30.0);
    EXPECT_EQ(std::get<std::string>(sheet.get_value({1, 2})), "thirty");
}

TEST_F(SortTest, UndoRestoresOriginalOrder) {
    sheet.set_value({0, 0}, 30.0);
    sheet.set_value({0, 1}, 10.0);
    sheet.set_value({0, 2}, 20.0);

    SortColumnCommand cmd(0, true, sheet);
    cmd.execute(sheet);

    // Verify sorted
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 10.0);

    // Undo
    cmd.undo(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 30.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 10.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 2})), 20.0);
}

TEST_F(SortTest, FormulasPermutedWithRows) {
    sheet.set_value({0, 0}, 30.0);
    sheet.set_formula({0, 0}, "B1+1");
    sheet.set_value({0, 1}, 10.0);
    sheet.set_value({0, 2}, 20.0);
    sheet.set_formula({0, 2}, "B3*2");

    SortColumnCommand cmd(0, true, sheet);
    cmd.execute(sheet);

    // Row that was at index 1 (value 10) is now at index 0 — no formula
    EXPECT_FALSE(sheet.has_formula({0, 0}));
    // Row that was at index 2 (value 20, formula "B3*2") is now at index 1
    EXPECT_TRUE(sheet.has_formula({0, 1}));
    EXPECT_EQ(sheet.get_formula({0, 1}), "B3*2");
    // Row that was at index 0 (value 30, formula "B1+1") is now at index 2
    EXPECT_TRUE(sheet.has_formula({0, 2}));
    EXPECT_EQ(sheet.get_formula({0, 2}), "B1+1");
}

TEST_F(SortTest, UndoManagerIntegration) {
    sheet.set_value({0, 0}, 30.0);
    sheet.set_value({0, 1}, 10.0);

    UndoManager undo;
    auto cmd = std::make_unique<SortColumnCommand>(0, true, sheet);
    undo.execute(std::move(cmd), sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 10.0);
    EXPECT_TRUE(undo.can_undo());

    undo.undo(sheet);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 30.0);

    undo.redo(sheet);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 10.0);
}
