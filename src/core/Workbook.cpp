#include "core/Workbook.hpp"

namespace magic {

Workbook::Workbook() {
    sheets_.emplace_back("Sheet1");
}

Sheet& Workbook::active_sheet() {
    return sheets_[active_];
}

const Sheet& Workbook::active_sheet() const {
    return sheets_[active_];
}

void Workbook::set_active(int idx) {
    if (idx >= 0 && idx < static_cast<int>(sheets_.size()))
        active_ = idx;
}

Sheet& Workbook::add_sheet(const std::string& name) {
    std::string n = name.empty()
        ? "Sheet" + std::to_string(sheets_.size() + 1)
        : name;
    sheets_.emplace_back(n);
    return sheets_.back();
}

void Workbook::remove_sheet(int idx) {
    if (sheet_count() <= 1) return;
    sheets_.erase(sheets_.begin() + idx);
    if (active_ >= sheet_count()) active_ = sheet_count() - 1;
}

}  // namespace magic
