#include <gtest/gtest.h>
#include "core/Column.hpp"

using namespace magic;

TEST(Column, SetGetDouble) {
    Column col;
    col.set(0, CellValue{42.0});
    col.set(1, CellValue{3.14});

    auto v0 = col.get(0);
    ASSERT_TRUE(is_number(v0));
    EXPECT_DOUBLE_EQ(as_number(v0), 42.0);

    auto v1 = col.get(1);
    ASSERT_TRUE(is_number(v1));
    EXPECT_DOUBLE_EQ(as_number(v1), 3.14);
}

TEST(Column, SetGetString) {
    Column col;
    col.set(0, CellValue{std::string("hello")});

    auto v = col.get(0);
    ASSERT_TRUE(is_string(v));
    EXPECT_EQ(as_string(v), "hello");
}

TEST(Column, EmptyCell) {
    Column col;
    auto v = col.get(100);
    EXPECT_TRUE(is_empty(v));
}

TEST(Column, Sum) {
    Column col;
    for (int i = 0; i < 10; ++i) {
        col.set(i, CellValue{static_cast<double>(i + 1)});
    }
    EXPECT_DOUBLE_EQ(col.sum(0, 10), 55.0);
    EXPECT_DOUBLE_EQ(col.sum(0, 5), 15.0);
}

TEST(Column, Clear) {
    Column col;
    col.set(0, CellValue{42.0});
    col.clear(0);
    // After clear, doubles_[0] is 0.0, no override
    auto v = col.get(0);
    ASSERT_TRUE(is_number(v));
    EXPECT_DOUBLE_EQ(as_number(v), 0.0);
}
