#pragma once
#include <chrono>
#include <filesystem>

namespace magic {

class Workbook;

// Periodic auto-save to a recovery file.
// On clean exit, the recovery file is deleted.
// On startup, check has_recovery() and offer to restore.
class AutoSave {
public:
    // Interval between auto-save checks (default: 60 seconds)
    static constexpr auto kInterval = std::chrono::seconds(60);

    // Check if a recovery file exists from a previous crash.
    [[nodiscard]] static bool has_recovery();

    // Return the path to the recovery file.
    [[nodiscard]] static std::filesystem::path recovery_path();

    // Delete the recovery file (after restore or discard).
    static void discard_recovery();

    // Called each frame from the main loop. Saves if dirty and enough
    // time has elapsed since the last save.
    void poll(const Workbook& workbook, bool is_dirty);

    // Force a save now (e.g., before risky operations).
    void save_now(const Workbook& workbook);

private:
    std::chrono::steady_clock::time_point last_save_{std::chrono::steady_clock::now()};
};

}  // namespace magic
