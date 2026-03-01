#include <gtest/gtest.h>
#include "core/Clipboard.hpp"
#include "core/Sheet.hpp"

using namespace magic;

TEST(Clipboard, CopySingle) {
    Sheet sheet("Test");
    sheet.set_value({0, 0}, CellValue{42.0});

    Clipboard cb;
    cb.copy_single(sheet, {0, 0});
    EXPECT_TRUE(cb.has_data());

    auto entries = cb.paste_at({1, 0});
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].addr, (CellAddress{1, 0}));
    EXPECT_DOUBLE_EQ(as_number(entries[0].value), 42.0);
}

TEST(Clipboard, AdjustReferences) {
    // Moving A1+B1 one row down → A2+B2
    std::string adjusted = Clipboard::adjust_references("A1+B1", 0, 1);
    EXPECT_EQ(adjusted, "A2+B2");

    // Moving A1 one column right → B1
    adjusted = Clipboard::adjust_references("A1", 1, 0);
    EXPECT_EQ(adjusted, "B1");
}

TEST(Clipboard, CopyFormula) {
    Sheet sheet("Test");
    sheet.set_value({0, 0}, CellValue{10.0});
    sheet.set_formula({1, 0}, "A1*2");
    sheet.set_value({1, 0}, CellValue{20.0});

    Clipboard cb;
    cb.copy_single(sheet, {1, 0});

    // Paste to B2 (one row down) → formula becomes A2*2
    auto entries = cb.paste_at({1, 1});
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].formula, "A2*2");
}

TEST(Clipboard, AdjustRange) {
    std::string adjusted = Clipboard::adjust_references("SUM(A1:A10)", 1, 0);
    EXPECT_EQ(adjusted, "SUM(B1:B10)");
}
