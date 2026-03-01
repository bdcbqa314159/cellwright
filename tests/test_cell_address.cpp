#include <gtest/gtest.h>
#include "core/CellAddress.hpp"

using namespace magic;

TEST(CellAddress, ColToLetters) {
    EXPECT_EQ(CellAddress::col_to_letters(0), "A");
    EXPECT_EQ(CellAddress::col_to_letters(1), "B");
    EXPECT_EQ(CellAddress::col_to_letters(25), "Z");
    EXPECT_EQ(CellAddress::col_to_letters(26), "AA");
    EXPECT_EQ(CellAddress::col_to_letters(27), "AB");
    EXPECT_EQ(CellAddress::col_to_letters(701), "ZZ");
}

TEST(CellAddress, LettersToCol) {
    EXPECT_EQ(CellAddress::letters_to_col("A"), 0);
    EXPECT_EQ(CellAddress::letters_to_col("B"), 1);
    EXPECT_EQ(CellAddress::letters_to_col("Z"), 25);
    EXPECT_EQ(CellAddress::letters_to_col("AA"), 26);
    EXPECT_EQ(CellAddress::letters_to_col("AB"), 27);
    EXPECT_EQ(CellAddress::letters_to_col("ZZ"), 701);
}

TEST(CellAddress, FromA1) {
    auto addr = CellAddress::from_a1("A1");
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->col, 0);
    EXPECT_EQ(addr->row, 0);

    addr = CellAddress::from_a1("B3");
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->col, 1);
    EXPECT_EQ(addr->row, 2);

    addr = CellAddress::from_a1("AA10");
    ASSERT_TRUE(addr.has_value());
    EXPECT_EQ(addr->col, 26);
    EXPECT_EQ(addr->row, 9);
}

TEST(CellAddress, FromA1Invalid) {
    EXPECT_FALSE(CellAddress::from_a1("").has_value());
    EXPECT_FALSE(CellAddress::from_a1("123").has_value());
    EXPECT_FALSE(CellAddress::from_a1("ABC").has_value());
}

TEST(CellAddress, ToA1) {
    EXPECT_EQ((CellAddress{0, 0}.to_a1()), "A1");
    EXPECT_EQ((CellAddress{1, 2}.to_a1()), "B3");
    EXPECT_EQ((CellAddress{26, 9}.to_a1()), "AA10");
}

TEST(CellAddress, Roundtrip) {
    for (int col = 0; col < 100; ++col) {
        for (int row = 0; row < 10; ++row) {
            CellAddress orig{col, row};
            auto parsed = CellAddress::from_a1(orig.to_a1());
            ASSERT_TRUE(parsed.has_value());
            EXPECT_EQ(*parsed, orig);
        }
    }
}
