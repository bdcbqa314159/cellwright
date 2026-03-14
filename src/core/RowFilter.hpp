#pragma once
#include "core/CellValue.hpp"
#include "core/Sheet.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace magic {

// Per-column text filter. Rows are visible if they match ALL active filters.
class RowFilter {
public:
    void set_filter(int32_t col, const std::string& text) {
        if (text.empty())
            filters_.erase(col);
        else
            filters_[col] = text;
        dirty_ = true;
    }

    void clear_filter(int32_t col) {
        filters_.erase(col);
        dirty_ = true;
    }

    void clear_all() {
        filters_.clear();
        dirty_ = true;
    }

    [[nodiscard]] bool has_filters() const { return !filters_.empty(); }

    [[nodiscard]] const std::string& get_filter(int32_t col) const {
        static const std::string empty;
        auto it = filters_.find(col);
        return it != filters_.end() ? it->second : empty;
    }

    // Rebuild the visible row list if filters or data changed.
    // Returns the visible row indices (all rows if no filters).
    const std::vector<int32_t>& update(const Sheet& sheet, uint64_t data_generation) {
        if (!dirty_ && data_generation == last_gen_)
            return visible_rows_;

        dirty_ = false;
        last_gen_ = data_generation;
        visible_rows_.clear();

        if (filters_.empty()) {
            // No filters — all rows visible
            int32_t n = sheet.row_count();
            visible_rows_.reserve(n);
            for (int32_t r = 0; r < n; ++r)
                visible_rows_.push_back(r);
            return visible_rows_;
        }

        int32_t n = sheet.row_count();
        visible_rows_.reserve(n);
        for (int32_t r = 0; r < n; ++r) {
            bool match = true;
            for (const auto& [col, filter_text] : filters_) {
                CellValue val = sheet.get_value({col, r});
                std::string display = to_display_string(val);
                // Case-insensitive substring match
                if (!icontains(display, filter_text)) {
                    match = false;
                    break;
                }
            }
            if (match) visible_rows_.push_back(r);
        }
        return visible_rows_;
    }

    void mark_dirty() { dirty_ = true; }

private:
    std::unordered_map<int32_t, std::string> filters_;
    std::vector<int32_t> visible_rows_;
    uint64_t last_gen_ = 0;
    bool dirty_ = true;

    static bool icontains(const std::string& haystack, const std::string& needle) {
        if (needle.empty()) return true;
        if (haystack.size() < needle.size()) return false;
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
        return it != haystack.end();
    }
};

}  // namespace magic
