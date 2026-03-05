#pragma once
#include "core/Sheet.hpp"
#include <algorithm>
#include <vector>
#include <string>

namespace magic {

class Workbook {
public:
    Workbook();

    Sheet& active_sheet();
    const Sheet& active_sheet() const;

    int active_index() const { return active_; }
    void set_active(int idx);

    Sheet& sheet(int idx) { return sheets_[std::clamp(idx, 0, static_cast<int>(sheets_.size()) - 1)]; }
    const Sheet& sheet(int idx) const { return sheets_[std::clamp(idx, 0, static_cast<int>(sheets_.size()) - 1)]; }
    int sheet_count() const { return static_cast<int>(sheets_.size()); }

    int add_sheet(const std::string& name = "");
    void remove_sheet(int idx);

private:
    std::vector<Sheet> sheets_;
    int active_ = 0;
};

}  // namespace magic
