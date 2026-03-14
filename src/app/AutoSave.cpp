#include "app/AutoSave.hpp"
#include "io/WorkbookIO.hpp"
#include "util/ConfigDir.hpp"
#include <iostream>

namespace magic {

std::filesystem::path AutoSave::recovery_path() {
    return std::filesystem::path(get_config_dir()) / ".cellwright_recovery.magic";
}

bool AutoSave::has_recovery() {
    std::error_code ec;
    return std::filesystem::exists(recovery_path(), ec) && !ec;
}

void AutoSave::discard_recovery() {
    std::error_code ec;
    std::filesystem::remove(recovery_path(), ec);
}

void AutoSave::poll(const Workbook& workbook, bool is_dirty) {
    if (!is_dirty) return;

    auto now = std::chrono::steady_clock::now();
    if (now - last_save_ < kInterval) return;

    save_now(workbook);
}

void AutoSave::save_now(const Workbook& workbook) {
    last_save_ = std::chrono::steady_clock::now();
    if (!WorkbookIO::save(recovery_path(), workbook)) {
        std::cerr << "[AutoSave] Failed to write recovery file\n";
    }
}

}  // namespace magic
