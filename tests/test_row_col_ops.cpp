#include <gtest/gtest.h>
#include "core/Sheet.hpp"
#include "core/Command.hpp"
#include "core/FormulaAdjust.hpp"
#include "formula/DependencyGraph.hpp"

using namespace magic;

// ── FormulaAdjust free functions ──────────────────────────────────────────────

TEST(FormulaAdjust, InsertRowShiftsDown) {
    // =A3 with insert at row 1 → =A4 (row 2 becomes row 3)
    auto result = adjust_formula_for_insert_row("=A3", 1);
    EXPECT_EQ(result, "=A4");
}

TEST(FormulaAdjust, InsertRowNoChangeAbove) {
    // =A1 with insert at row 2 → =A1 (row 0 is above)
    auto result = adjust_formula_for_insert_row("=A1", 2);
    EXPECT_EQ(result, "=A1");
}

TEST(FormulaAdjust, DeleteRowProducesRef) {
    // =A2 with delete at row 1 → #REF! (row 1 is deleted)
    auto result = adjust_formula_for_delete_row("=A2", 1);
    EXPECT_NE(result.find("#REF!"), std::string::npos);
}

TEST(FormulaAdjust, DeleteRowShiftsUp) {
    // =A4 with delete at row 1 → =A3 (row 3 shifts to row 2)
    auto result = adjust_formula_for_delete_row("=A4", 1);
    EXPECT_EQ(result, "=A3");
}

TEST(FormulaAdjust, InsertColShiftsRight) {
    // =B1 with insert at col 0 → =C1
    auto result = adjust_formula_for_insert_col("=B1", 0);
    EXPECT_EQ(result, "=C1");
}

TEST(FormulaAdjust, DeleteColProducesRef) {
    // =B1 with delete at col 1 → #REF!
    auto result = adjust_formula_for_delete_col("=B1", 1);
    EXPECT_NE(result.find("#REF!"), std::string::npos);
}

TEST(FormulaAdjust, MultipleRefs) {
    // =A2+B3 with insert row at 1 → =A3+B4
    auto result = adjust_formula_for_insert_row("=A2+B3", 1);
    EXPECT_EQ(result, "=A3+B4");
}

// ── Sheet insert/delete row ──────────────────────────────────────────────────

TEST(Sheet, InsertRowShiftsValues) {
    Sheet sheet("Test", 2, 5);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({0, 1}, CellValue{2.0});
    sheet.set_value({0, 2}, CellValue{3.0});

    sheet.insert_row(1);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_TRUE(is_empty(sheet.get_value({0, 1})));  // new empty row
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 2})), 2.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 3})), 3.0);
}

TEST(Sheet, DeleteRowShiftsValues) {
    Sheet sheet("Test", 2, 5);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({0, 1}, CellValue{2.0});
    sheet.set_value({0, 2}, CellValue{3.0});

    sheet.delete_row(1);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 3.0);
}

TEST(Sheet, InsertRowShiftsFormulas) {
    Sheet sheet("Test", 2, 5);
    sheet.set_formula({0, 2}, "=A2");  // A3 references A2

    sheet.insert_row(1);

    // Formula key shifted from row 2 → row 3
    EXPECT_TRUE(sheet.has_formula({0, 3}));
    // Formula text adjusted: =A2 → =A3
    EXPECT_EQ(sheet.get_formula({0, 3}), "=A3");
}

TEST(Sheet, DeleteRowAdjustsFormulas) {
    Sheet sheet("Test", 2, 5);
    sheet.set_formula({0, 3}, "=A3");  // A4 references A3

    sheet.delete_row(1);

    // Formula key shifted from row 3 → row 2
    EXPECT_TRUE(sheet.has_formula({0, 2}));
    // Formula text adjusted: =A3 → =A2
    EXPECT_EQ(sheet.get_formula({0, 2}), "=A2");
}

TEST(Sheet, InsertColumnShiftsValues) {
    Sheet sheet("Test", 3, 3);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({1, 0}, CellValue{2.0});

    sheet.insert_column(1);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_TRUE(is_empty(sheet.get_value({1, 0})));  // new empty column
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({2, 0})), 2.0);
}

TEST(Sheet, DeleteColumnShiftsValues) {
    Sheet sheet("Test", 3, 3);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({1, 0}, CellValue{2.0});
    sheet.set_value({2, 0}, CellValue{3.0});

    sheet.delete_column(1);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({1, 0})), 3.0);
}

// ── Insert/Delete undo via Command ──────────────────────────────────────────

TEST(InsertRowCommand, UndoRestoresState) {
    Sheet sheet("Test", 2, 5);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({0, 1}, CellValue{2.0});

    UndoManager mgr;
    auto cmd = std::make_unique<InsertRowCommand>(1);
    mgr.execute(std::move(cmd), sheet);

    EXPECT_TRUE(is_empty(sheet.get_value({0, 1})));
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 2})), 2.0);

    mgr.undo(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 2.0);
}

TEST(DeleteRowCommand, UndoRestoresRowAndFormulas) {
    Sheet sheet("Test", 2, 5);
    sheet.set_value({0, 0}, CellValue{10.0});
    sheet.set_value({0, 1}, CellValue{20.0});
    sheet.set_formula({0, 2}, "=A2");

    UndoManager mgr;
    auto cmd = std::make_unique<DeleteRowCommand>(1, sheet);
    mgr.execute(std::move(cmd), sheet);

    // Row 1 deleted; row 2 shifted to row 1
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 10.0);
    // Formula key shifted and text adjusted
    EXPECT_TRUE(sheet.has_formula({0, 1}));

    mgr.undo(sheet);

    // Original state restored
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 20.0);
    EXPECT_TRUE(sheet.has_formula({0, 2}));
    EXPECT_EQ(sheet.get_formula({0, 2}), "=A2");
}

TEST(DeleteColumnCommand, UndoRestoresColumn) {
    Sheet sheet("Test", 3, 3);
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({1, 0}, CellValue{2.0});
    sheet.set_value({2, 0}, CellValue{3.0});

    UndoManager mgr;
    auto cmd = std::make_unique<DeleteColumnCommand>(1, sheet);
    mgr.execute(std::move(cmd), sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({1, 0})), 3.0);

    mgr.undo(sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({1, 0})), 2.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({2, 0})), 3.0);
}

// ── DependencyGraph shift ─────────────────────────────────────────────────────

TEST(DependencyGraph, ShiftRowsInsert) {
    DependencyGraph graph;
    CellAddress a1{0, 0};  // A1
    CellAddress a3{0, 2};  // A3

    graph.set_dependencies(a3, {a1});

    // Insert row at row 1: A3 becomes A4
    graph.shift_rows(1, 1);

    CellAddress a4{0, 3};
    auto deps = graph.dependents_of(a1);
    ASSERT_EQ(deps.size(), 1u);
    EXPECT_EQ(deps[0], a4);
}

TEST(DependencyGraph, ShiftRowsDeleteDropsEntry) {
    DependencyGraph graph;
    CellAddress a1{0, 0};
    CellAddress a2{0, 1};

    graph.set_dependencies(a2, {a1});

    // Delete row 1: A2 is removed
    graph.shift_rows(1, -1);

    auto deps = graph.dependents_of(a1);
    EXPECT_TRUE(deps.empty());
}

TEST(DependencyGraph, ShiftColsInsert) {
    DependencyGraph graph;
    CellAddress a1{0, 0};
    CellAddress b1{1, 0};

    graph.set_dependencies(b1, {a1});

    // Insert column at col 1: B1 becomes C1
    graph.shift_cols(1, 1);

    CellAddress c1{2, 0};
    auto deps = graph.dependents_of(a1);
    ASSERT_EQ(deps.size(), 1u);
    EXPECT_EQ(deps[0], c1);
}
