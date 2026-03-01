#pragma once
#include "core/CellValue.hpp"
#include "core/CellAddress.hpp"
#include <string>
#include <unordered_map>

namespace magic {

enum class FormatType {
    GENERAL,     // Auto-detect
    NUMBER,      // Fixed decimal places
    PERCENTAGE,  // Multiply by 100, show %
    CURRENCY,    // Prefix $
    SCIENTIFIC,  // 1.23E+04
    DATE,        // ISO date (YYYY-MM-DD)
};

struct CellFormat {
    FormatType type = FormatType::GENERAL;
    int decimals = 2;
    std::string currency_symbol = "$";
    std::string date_input_hint;  // Tooltip text for date cells

    bool operator==(const CellFormat&) const = default;
};

// Apply a format to a cell value for display
std::string format_value(const CellValue& val, const CellFormat& fmt);

// Format store for a sheet
class FormatMap {
public:
    void set(const CellAddress& addr, const CellFormat& fmt);
    CellFormat get(const CellAddress& addr) const;
    void clear(const CellAddress& addr);
    bool has(const CellAddress& addr) const;

private:
    std::unordered_map<CellAddress, CellFormat> formats_;
};

}  // namespace magic
