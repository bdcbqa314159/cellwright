#include "core/CellFormat.hpp"
#include "core/DateSerial.hpp"
#include <algorithm>
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
    int decimals = std::clamp(fmt.decimals, 0, 20);

    switch (fmt.type) {
        case FormatType::GENERAL:
            return to_display_string(val);

        case FormatType::NUMBER: {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%.*f", decimals, d);
            return buf;
        }

        case FormatType::PERCENTAGE: {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%.*f%%", decimals, d * 100.0);
            return buf;
        }

        case FormatType::CURRENCY: {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s%.*f", fmt.currency_symbol.c_str(), decimals, std::abs(d));
            if (d < 0) return std::string("-") + buf;
            return buf;
        }

        case FormatType::SCIENTIFIC: {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%.*e", decimals, d);
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
    return formats_.contains(addr);
}

void FormatMap::shift_rows(int32_t at, int32_t delta) {
    std::unordered_map<CellAddress, CellFormat> shifted;
    for (auto& [addr, fmt] : formats_) {
        if (delta < 0 && addr.row == at) continue;  // deleted
        if (addr.row >= at)
            shifted[{addr.col, addr.row + delta}] = std::move(fmt);
        else
            shifted[addr] = std::move(fmt);
    }
    formats_ = std::move(shifted);
}

void FormatMap::shift_cols(int32_t at, int32_t delta) {
    std::unordered_map<CellAddress, CellFormat> shifted;
    for (auto& [addr, fmt] : formats_) {
        if (delta < 0 && addr.col == at) continue;  // deleted
        if (addr.col >= at)
            shifted[{addr.col + delta, addr.row}] = std::move(fmt);
        else
            shifted[addr] = std::move(fmt);
    }
    formats_ = std::move(shifted);
}

}  // namespace magic
