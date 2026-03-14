#pragma once
#include "core/Sheet.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>

namespace magic {

class Workbook {
public:
    Workbook();

    Sheet& active_sheet();
    const Sheet& active_sheet() const;

    int active_index() const { return active_; }
    void set_active(int idx);

    // Access sheet by index. Out-of-range indices are silently clamped to [0, sheet_count()-1].
    Sheet& sheet(int idx) { return sheets_[std::clamp(idx, 0, static_cast<int>(sheets_.size()) - 1)]; }
    const Sheet& sheet(int idx) const { return sheets_[std::clamp(idx, 0, static_cast<int>(sheets_.size()) - 1)]; }

    // Checked accessor — throws std::out_of_range for invalid indices.
    Sheet& sheet_checked(int idx) {
        if (idx < 0 || idx >= static_cast<int>(sheets_.size()))
            throw std::out_of_range("Workbook::sheet_checked: index out of range");
        return sheets_[idx];
    }
    const Sheet& sheet_checked(int idx) const {
        if (idx < 0 || idx >= static_cast<int>(sheets_.size()))
            throw std::out_of_range("Workbook::sheet_checked: index out of range");
        return sheets_[idx];
    }
    int sheet_count() const { return static_cast<int>(sheets_.size()); }

    int add_sheet(const std::string& name = "");
    void remove_sheet(int idx);

private:
    std::vector<Sheet> sheets_;
    int active_ = 0;
};

}  // namespace magic
