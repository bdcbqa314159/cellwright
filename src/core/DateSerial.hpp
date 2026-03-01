#pragma once
#include <optional>
#include <string>

namespace magic {

struct DateParseResult {
    double serial;          // Days since Unix epoch (1970-01-01)
    std::string iso;        // "YYYY-MM-DD"
    std::string input_hint; // e.g. "Parsed as MM/DD/YYYY → 2024-12-12"
};

// Try to parse a string as a date. Returns nullopt if not a valid date.
// Priority: ISO (YYYY-MM-DD), US slash (MM/DD/YYYY), EU dot (DD.MM.YYYY),
//           dash (MM-DD-YYYY), month names (December 12, 2024 / 12 Dec 2024).
std::optional<DateParseResult> parse_date(const std::string& input);

// Convert a serial (days since epoch) back to "YYYY-MM-DD".
std::string serial_to_iso(double serial);

}  // namespace magic
