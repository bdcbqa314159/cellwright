#pragma once
#include <string>
#include <vector>

namespace magic {

struct WindowRect {
    int x = -1, y = -1, w = 1400, h = 900;
};

class Settings {
public:
    // Load from ~/.cellwright.json. Returns false if file not found.
    bool load();
    bool save() const;

    // Recent files (most recent first, max 10)
    const std::vector<std::string>& recent_files() const { return recent_files_; }
    void add_recent_file(const std::string& path);

    WindowRect window_rect;
    float font_size = 16.0f;

private:
    std::vector<std::string> recent_files_;
    static std::string settings_path();
};

}  // namespace magic
