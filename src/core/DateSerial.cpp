#include "core/DateSerial.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <regex>
#include <algorithm>
#include <cctype>

namespace magic {

// ── Helpers ──────────────────────────────────────────────────────────────────

static int expand_year(int y) {
    if (y >= 100) return y;
    return (y <= 49) ? 2000 + y : 1900 + y;
}

static bool is_valid_date(int y, int m, int d) {
    using namespace std::chrono;
    year_month_day ymd{year{y}, month{static_cast<unsigned>(m)},
                       day{static_cast<unsigned>(d)}};
    return ymd.ok();
}

static double to_serial(int y, int m, int d) {
    using namespace std::chrono;
    sys_days sd = year_month_day{year{y}, month{static_cast<unsigned>(m)},
                                 day{static_cast<unsigned>(d)}};
    return static_cast<double>(sd.time_since_epoch().count());
}

static std::string to_iso(int y, int m, int d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return buf;
}

static std::optional<DateParseResult> make_result(int y, int m, int d,
                                                   const std::string& hint_fmt,
                                                   const std::string& /*input*/) {
    y = expand_year(y);
    if (!is_valid_date(y, m, d)) return std::nullopt;
    std::string iso = to_iso(y, m, d);
    DateParseResult r;
    r.serial = to_serial(y, m, d);
    r.iso = iso;
    r.input_hint = "Parsed as " + hint_fmt + " \xe2\x86\x92 " + iso;  // → (UTF-8)
    return r;
}

// ── Month name lookup ────────────────────────────────────────────────────────

static const char* month_names[] = {
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december"
};
static const char* month_abbrevs[] = {
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec"
};

static int month_from_name(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (int i = 0; i < 12; ++i) {
        if (lower == month_names[i] || lower == month_abbrevs[i])
            return i + 1;
    }
    return 0;
}

// ── Compiled regex patterns ─────────────────────────────────────────────────

// ISO: YYYY-MM-DD
static const std::regex re_iso(R"(^(\d{4})-(\d{1,2})-(\d{1,2})$)");

// US slash: M/D/YYYY or M/D/YY
static const std::regex re_us_slash(R"(^(\d{1,2})/(\d{1,2})/(\d{2,4})$)");

// EU dot: D.M.YYYY or D.M.YY
static const std::regex re_eu_dot(R"(^(\d{1,2})\.(\d{1,2})\.(\d{2,4})$)");

// Dash with 1-2 digit first: M-D-YYYY or M-D-YY  (4-digit first caught by ISO)
static const std::regex re_dash_us(R"(^(\d{1,2})-(\d{1,2})-(\d{2,4})$)");

// Month name first: December 12, 2024 / Dec 12, 2024
static const std::regex re_month_first(
    R"(^([A-Za-z]+)\s+(\d{1,2}),?\s+(\d{2,4})$)");

// Day first: 12 December 2024 / 12 Dec 2024
static const std::regex re_day_first(
    R"(^(\d{1,2})\s+([A-Za-z]+)\s+(\d{2,4})$)");

// ── Public API ──────────────────────────────────────────────────────────────

std::optional<DateParseResult> parse_date(const std::string& input) {
    // Trim whitespace
    auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::nullopt;
    auto end = input.find_last_not_of(" \t\r\n");
    std::string s = input.substr(start, end - start + 1);

    std::smatch m;

    // 1. ISO: YYYY-MM-DD
    if (std::regex_match(s, m, re_iso)) {
        int y = std::stoi(m[1]);
        int mo = std::stoi(m[2]);
        int d = std::stoi(m[3]);
        return make_result(y, mo, d, "YYYY-MM-DD", s);
    }

    // 2. US slash: MM/DD/YYYY
    if (std::regex_match(s, m, re_us_slash)) {
        int mo = std::stoi(m[1]);
        int d = std::stoi(m[2]);
        int y = std::stoi(m[3]);
        std::string hint = (m[3].length() <= 2) ? "MM/DD/YY" : "MM/DD/YYYY";
        return make_result(y, mo, d, hint, s);
    }

    // 3. EU dot: DD.MM.YYYY
    if (std::regex_match(s, m, re_eu_dot)) {
        int d = std::stoi(m[1]);
        int mo = std::stoi(m[2]);
        int y = std::stoi(m[3]);
        std::string hint = (m[3].length() <= 2) ? "DD.MM.YY" : "DD.MM.YYYY";
        return make_result(y, mo, d, hint, s);
    }

    // 4. Dash US: MM-DD-YYYY (1-2 digit first)
    if (std::regex_match(s, m, re_dash_us)) {
        int mo = std::stoi(m[1]);
        int d = std::stoi(m[2]);
        int y = std::stoi(m[3]);
        std::string hint = (m[3].length() <= 2) ? "MM-DD-YY" : "MM-DD-YYYY";
        return make_result(y, mo, d, hint, s);
    }

    // 5a. Month name first: December 12, 2024
    if (std::regex_match(s, m, re_month_first)) {
        int mo = month_from_name(m[1]);
        if (mo > 0) {
            int d = std::stoi(m[2]);
            int y = std::stoi(m[3]);
            return make_result(y, mo, d, "Month DD, YYYY", s);
        }
    }

    // 5b. Day first: 12 December 2024
    if (std::regex_match(s, m, re_day_first)) {
        int mo = month_from_name(m[2]);
        if (mo > 0) {
            int d = std::stoi(m[1]);
            int y = std::stoi(m[3]);
            return make_result(y, mo, d, "DD Month YYYY", s);
        }
    }

    return std::nullopt;
}

std::string serial_to_iso(double serial) {
    using namespace std::chrono;
    if (!std::isfinite(serial) || serial < -365243219162.0 || serial > 365241780471.0)
        return "";
    auto days_count = static_cast<int>(serial);
    sys_days sd{days{days_count}};
    year_month_day ymd{sd};
    if (!ymd.ok()) return "";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()));
    return buf;
}

}  // namespace magic
