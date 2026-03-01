#include <gtest/gtest.h>
#include "core/CellFormat.hpp"

using namespace magic;

TEST(CellFormat, General) {
    CellFormat fmt;
    EXPECT_EQ(format_value(CellValue{42.0}, fmt), "42");
    EXPECT_EQ(format_value(CellValue{3.14}, fmt), "3.14");
    EXPECT_EQ(format_value(CellValue{}, fmt), "");
}

TEST(CellFormat, NumberFormat) {
    CellFormat fmt{FormatType::NUMBER, 2};
    EXPECT_EQ(format_value(CellValue{42.0}, fmt), "42.00");
    EXPECT_EQ(format_value(CellValue{3.14159}, fmt), "3.14");
}

TEST(CellFormat, Percentage) {
    CellFormat fmt{FormatType::PERCENTAGE, 1};
    EXPECT_EQ(format_value(CellValue{0.75}, fmt), "75.0%");
    EXPECT_EQ(format_value(CellValue{1.0}, fmt), "100.0%");
}

TEST(CellFormat, Currency) {
    CellFormat fmt{FormatType::CURRENCY, 2, "$"};
    EXPECT_EQ(format_value(CellValue{42.0}, fmt), "$42.00");
    EXPECT_EQ(format_value(CellValue{-5.0}, fmt), "-$5.00");
}

TEST(CellFormat, Scientific) {
    CellFormat fmt{FormatType::SCIENTIFIC, 2};
    std::string result = format_value(CellValue{12345.0}, fmt);
    EXPECT_TRUE(result.find("e") != std::string::npos || result.find("E") != std::string::npos);
}

TEST(CellFormat, FormatMap) {
    FormatMap map;
    CellAddress a1{0, 0};

    EXPECT_EQ(map.get(a1).type, FormatType::GENERAL);

    map.set(a1, {FormatType::PERCENTAGE, 1});
    EXPECT_EQ(map.get(a1).type, FormatType::PERCENTAGE);

    map.clear(a1);
    EXPECT_EQ(map.get(a1).type, FormatType::GENERAL);
}
