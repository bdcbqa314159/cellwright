#include <gtest/gtest.h>
#include "io/CsvIO.hpp"

#include <cmath>
#include <string>
#include <variant>
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

// ── Number-vs-text classification ───────────────────────────────────────────
//
// Characterization tests: these pin the EXISTING behaviour of CsvIO::parse so the
// switch from std::stod to std::strtod cannot change it silently. They pass both
// before and after that change — they are a refactor safety net, not failing-first
// tests, because the refactor is not meant to alter behaviour.
//
// The subtle cases are the out-of-range ones: std::stod throws std::out_of_range on
// both overflow and underflow, and the bare catch turned that into text. strtod
// signals the same condition through ERANGE instead.

namespace {

CellValue first_cell(const std::string& field) {
    Sheet sheet;
    CsvIO::parse(field + "\n", sheet);
    return sheet.get_value({0, 0});
}

void expect_number(const std::string& field, double expected) {
    const CellValue v = first_cell(field);
    const auto* d = std::get_if<double>(&v);
    ASSERT_NE(d, nullptr) << "[" << field << "] expected a number";
    EXPECT_DOUBLE_EQ(*d, expected) << "[" << field << "]";
}

void expect_text(const std::string& field) {
    const CellValue v = first_cell(field);
    const auto* s = std::get_if<std::string>(&v);
    ASSERT_NE(s, nullptr) << "[" << field << "] expected text";
    EXPECT_EQ(*s, field);
}

}  // namespace

TEST(CsvIO, ParsesPlainNumbers) {
    expect_number("42", 42.0);
    expect_number("42.5", 42.5);
    expect_number("-3", -3.0);
    expect_number("+7", 7.0);
    expect_number(".5", 0.5);
    expect_number("5.", 5.0);
}

TEST(CsvIO, ParsesExponentAndHexForms) {
    expect_number("1e5", 100000.0);
    expect_number("1E5", 100000.0);
    expect_number("0x1A", 26.0);  // strtod/stod both accept hex
}

TEST(CsvIO, LeadingWhitespaceIsNumericButTrailingIsNot) {
    expect_number("  42", 42.0);  // leading space is skipped by the converter
    expect_text("42  ");          // trailing junk means the field was not fully consumed
}

TEST(CsvIO, NonNumericFieldsStayText) {
    expect_text("abc");
    expect_text("--5");
    expect_text("1.5abc");  // partial parse is not a number
    expect_text("true");
    expect_text("2026-10-27");
}

TEST(CsvIO, OutOfRangeMagnitudesStayText) {
    // std::stod threw out_of_range here; strtod reports ERANGE. Either way the
    // field must remain text rather than becoming inf or 0.
    expect_text("1e400");
    expect_text("1e-400");
}

TEST(CsvIO, InfinityParsesAsNumber) {
    const CellValue v = first_cell("inf");
    const auto* d = std::get_if<double>(&v);
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(std::isinf(*d));
    EXPECT_GT(*d, 0.0);
}

TEST(CsvIO, TextCellsDoNotThrowAcrossALargeFile) {
    // The motivating case: every non-numeric cell used to throw. 2000 text cells.
    std::string csv;
    for (int row = 0; row < 500; ++row) csv += "alpha,beta,gamma,delta\n";

    Sheet sheet;
    CsvIO::parse(csv, sheet);

    const CellValue top_left = sheet.get_value({0, 0});
    const auto* a = std::get_if<std::string>(&top_left);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(*a, "alpha");

    const CellValue bottom_right = sheet.get_value({3, 499});
    const auto* d = std::get_if<std::string>(&bottom_right);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(*d, "delta");
}
