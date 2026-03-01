#include <gtest/gtest.h>
#include "core/DateSerial.hpp"

using namespace magic;

// ── ISO format ──────────────────────────────────────────────────────────────

TEST(DateSerial, IsoFormat) {
    auto r = parse_date("2024-12-12");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
    EXPECT_NE(r->serial, 12.0);
}

TEST(DateSerial, IsoFormatEpoch) {
    auto r = parse_date("1970-01-01");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "1970-01-01");
    EXPECT_DOUBLE_EQ(r->serial, 0.0);
}

TEST(DateSerial, IsoFormatPreEpoch) {
    auto r = parse_date("1969-12-31");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "1969-12-31");
    EXPECT_DOUBLE_EQ(r->serial, -1.0);
}

// ── US slash format ─────────────────────────────────────────────────────────

TEST(DateSerial, UsSlash4Digit) {
    auto r = parse_date("12/12/2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
    EXPECT_NE(r->serial, 12.0);  // The original bug
}

TEST(DateSerial, UsSlash2Digit) {
    auto r = parse_date("01/15/24");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-01-15");
}

TEST(DateSerial, UsSlash2DigitOld) {
    auto r = parse_date("06/15/99");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "1999-06-15");
}

// ── EU dot format ───────────────────────────────────────────────────────────

TEST(DateSerial, EuDot4Digit) {
    auto r = parse_date("25.12.2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-25");
}

TEST(DateSerial, EuDot2Digit) {
    auto r = parse_date("25.12.24");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-25");
}

// ── Dash format ─────────────────────────────────────────────────────────────

TEST(DateSerial, DashUs4Digit) {
    auto r = parse_date("12-25-2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-25");
}

TEST(DateSerial, DashUs2Digit) {
    auto r = parse_date("12-25-24");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-25");
}

// ── Month name formats ──────────────────────────────────────────────────────

TEST(DateSerial, MonthNameLong) {
    auto r = parse_date("December 12, 2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
}

TEST(DateSerial, MonthNameShort) {
    auto r = parse_date("Dec 12, 2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
}

TEST(DateSerial, DayFirstMonthNameLong) {
    auto r = parse_date("12 December 2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
}

TEST(DateSerial, DayFirstMonthNameShort) {
    auto r = parse_date("12 Dec 2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
}

TEST(DateSerial, MonthNameNoComma) {
    auto r = parse_date("December 12 2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-12-12");
}

// ── 2-digit year expansion ──────────────────────────────────────────────────

TEST(DateSerial, TwoDigitYear2024) {
    auto r = parse_date("01/01/24");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2024-01-01");
}

TEST(DateSerial, TwoDigitYear1999) {
    auto r = parse_date("01/01/99");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "1999-01-01");
}

TEST(DateSerial, TwoDigitYear1950) {
    auto r = parse_date("01/01/50");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "1950-01-01");
}

TEST(DateSerial, TwoDigitYear2049) {
    auto r = parse_date("01/01/49");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->iso, "2049-01-01");
}

// ── Invalid dates rejected ──────────────────────────────────────────────────

TEST(DateSerial, InvalidFeb30) {
    EXPECT_FALSE(parse_date("02/30/2024").has_value());
}

TEST(DateSerial, InvalidMonth13) {
    EXPECT_FALSE(parse_date("13/01/2024").has_value());
}

TEST(DateSerial, InvalidDay32) {
    EXPECT_FALSE(parse_date("01/32/2024").has_value());
}

// ── Non-dates rejected ──────────────────────────────────────────────────────

TEST(DateSerial, PlainNumber) {
    EXPECT_FALSE(parse_date("42").has_value());
}

TEST(DateSerial, PlainText) {
    EXPECT_FALSE(parse_date("hello").has_value());
}

TEST(DateSerial, EmptyString) {
    EXPECT_FALSE(parse_date("").has_value());
}

TEST(DateSerial, PartialSlash) {
    EXPECT_FALSE(parse_date("12/2024").has_value());
}

// ── serial_to_iso roundtrip ─────────────────────────────────────────────────

TEST(DateSerial, SerialToIsoEpoch) {
    EXPECT_EQ(serial_to_iso(0.0), "1970-01-01");
}

TEST(DateSerial, SerialToIsoNegative) {
    EXPECT_EQ(serial_to_iso(-1.0), "1969-12-31");
}

TEST(DateSerial, Roundtrip) {
    auto r = parse_date("2024-12-25");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(serial_to_iso(r->serial), "2024-12-25");
}

// ── Input hint ──────────────────────────────────────────────────────────────

TEST(DateSerial, InputHintContainsFormat) {
    auto r = parse_date("12/12/2024");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->input_hint.find("MM/DD/YYYY") != std::string::npos);
    EXPECT_TRUE(r->input_hint.find("2024-12-12") != std::string::npos);
}
