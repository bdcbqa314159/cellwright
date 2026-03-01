#include <gtest/gtest.h>
#include "io/CsvIO.hpp"

using namespace magic;

TEST(CsvIO, ParseSimple) {
    Sheet sheet("Test");
    CsvIO::parse("1,2,3\n4,5,6\n", sheet);

    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 0})), 1.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({1, 0})), 2.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({2, 0})), 3.0);
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({0, 1})), 4.0);
}

TEST(CsvIO, ParseQuoted) {
    Sheet sheet("Test");
    CsvIO::parse("\"hello, world\",42\n", sheet);

    ASSERT_TRUE(is_string(sheet.get_value({0, 0})));
    EXPECT_EQ(as_string(sheet.get_value({0, 0})), "hello, world");
    EXPECT_DOUBLE_EQ(as_number(sheet.get_value({1, 0})), 42.0);
}

TEST(CsvIO, SerializeRoundtrip) {
    Sheet sheet("Test");
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({1, 0}, CellValue{std::string("hello")});
    sheet.set_value({0, 1}, CellValue{3.14});

    std::string csv = CsvIO::serialize(sheet);
    Sheet sheet2("Test2");
    CsvIO::parse(csv, sheet2);

    EXPECT_DOUBLE_EQ(as_number(sheet2.get_value({0, 0})), 1.0);
    EXPECT_EQ(as_string(sheet2.get_value({1, 0})), "hello");
    EXPECT_DOUBLE_EQ(as_number(sheet2.get_value({0, 1})), 3.14);
}
