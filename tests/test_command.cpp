#include <gtest/gtest.h>
#include "core/Command.hpp"
#include "core/Sheet.hpp"

using namespace magic;

TEST(UndoManager, UndoRedo) {
    Sheet sheet("Test");
    UndoManager mgr;

    // Set A1 = 42
    auto cmd = std::make_unique<SetValueCommand>(
        CellAddress{0, 0}, CellValue{42.0}, CellValue{}, "");
    mgr.execute(std::move(cmd), sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 42.0);

    // Undo
    mgr.undo(sheet);
    EXPECT_TRUE(is_empty(sheet.get_value({0, 0})));

    // Redo
    mgr.redo(sheet);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 42.0);
}

TEST(UndoManager, FormulaUndoRedo) {
    Sheet sheet("Test");
    UndoManager mgr;

    sheet.set_value({0, 0}, CellValue{10.0});

    auto cmd = std::make_unique<SetFormulaCommand>(
        CellAddress{1, 0}, "A1*2", CellValue{20.0}, CellValue{}, "");
    mgr.execute(std::move(cmd), sheet);

    EXPECT_EQ(sheet.get_formula({1, 0}), "A1*2");
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({1, 0})), 20.0);

    mgr.undo(sheet);
    EXPECT_FALSE(sheet.has_formula({1, 0}));
    EXPECT_TRUE(is_empty(sheet.get_value({1, 0})));
}

TEST(UndoManager, MultipleUndos) {
    Sheet sheet("Test");
    UndoManager mgr;

    for (int i = 0; i < 5; ++i) {
        auto cmd = std::make_unique<SetValueCommand>(
            CellAddress{0, 0}, CellValue{static_cast<double>(i + 1)},
            sheet.get_value({0, 0}), "");
        mgr.execute(std::move(cmd), sheet);
    }

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 5.0);

    mgr.undo(sheet);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 4.0);

    mgr.undo(sheet);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 3.0);
}
