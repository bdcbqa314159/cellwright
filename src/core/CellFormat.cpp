#include "core/CellFormat.hpp"
#include "core/DateSerial.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace magic {

std::string format_value(const CellValue& val, const CellFormat& fmt) {
    if (is_empty(val)) return "";
    if (is_error(val)) return to_display_string(val);
    if (is_string(val)) return as_string(val);
    if (is_bool(val)) return std::get<bool>(val) ? "TRUE" : "FALSE";

    if (!is_number(val)) return to_display_string(val);

    double d = as_number(val);

    switch (fmt.type) {
        case FormatType::GENERAL:
            return to_display_string(val);

        case FormatType::NUMBER: {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(fmt.decimals) << d;
            return oss.str();
        }

        case FormatType::PERCENTAGE: {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(fmt.decimals) << (d * 100.0) << "%";
            return oss.str();
        }

        case FormatType::CURRENCY: {
            std::ostringstream oss;
            oss << fmt.currency_symbol << std::fixed << std::setprecision(fmt.decimals) << std::abs(d);
            if (d < 0) return "-" + oss.str();
            return oss.str();
        }

        case FormatType::SCIENTIFIC: {
            std::ostringstream oss;
            oss << std::scientific << std::setprecision(fmt.decimals) << d;
            return oss.str();
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
