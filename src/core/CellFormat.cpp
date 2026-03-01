#include "core/CellFormat.hpp"
#include "core/DateSerial.hpp"
#include <cmath>
#include <cstdio>

namespace magic {

std::string format_value(const CellValue& val, const CellFormat& fmt) {
    if (is_empty(val)) return "";
    if (is_error(val)) return to_display_string(val);
    if (is_string(val)) return as_string(val);
    if (is_bool(val)) return std::get<bool>(val) ? "true" : "false";

    if (!is_number(val)) return to_display_string(val);

    double d = as_number(val);

    switch (fmt.type) {
        case FormatType::GENERAL:
            return to_display_string(val);

        case FormatType::NUMBER: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.*f", fmt.decimals, d);
            return buf;
        }

        case FormatType::PERCENTAGE: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.*f%%", fmt.decimals, d * 100.0);
            return buf;
        }

        case FormatType::CURRENCY: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s%.*f", fmt.currency_symbol.c_str(), fmt.decimals, std::abs(d));
            if (d < 0) return std::string("-") + buf;
            return buf;
        }

        case FormatType::SCIENTIFIC: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.*e", fmt.decimals, d);
            return buf;
        }

        case FormatType::DATE: {
            std::string iso = serial_to_iso(d);
            return iso.empty() ? to_display_string(val) : iso;
        }
    }

    return to_display_string(val);
}

void FormatMap::set(const CellAddress& addr, const CellFormat& fmt) {
    if (fmt.type == FormatType::GENERAL)
        formats_.erase(addr);
    else
        formats_[addr] = fmt;
}

CellFormat FormatMap::get(const CellAddress& addr) const {
    auto it = formats_.find(addr);
    if (it != formats_.end()) return it->second;
    return {};
}

void FormatMap::clear(const CellAddress& addr) {
    formats_.erase(addr);
}

bool FormatMap::has(const CellAddress& addr) const {
    return formats_.count(addr) > 0;
}

}  // namespace magic
