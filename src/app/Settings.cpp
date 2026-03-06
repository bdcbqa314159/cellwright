#include "app/Settings.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace magic {

std::string Settings::settings_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.magic_dashboard.json";
}

void Settings::add_recent_file(const std::string& path) {
    // Remove if already present
    recent_files_.erase(
        std::remove(recent_files_.begin(), recent_files_.end(), path),
        recent_files_.end());
    // Insert at front
    recent_files_.insert(recent_files_.begin(), path);
    // Keep max 10
    if (recent_files_.size() > 10)
        recent_files_.resize(10);
}

// Minimal JSON parser/writer (no dependencies needed)

static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            default: out += c; break;
        }
    }
    out += '"';
    return out;
}

bool Settings::save() const {
    std::string path = settings_path();
    if (path.empty()) return false;

    std::ofstream f(path);
    if (!f) return false;

    f << "{\n";
    f << "  \"window_x\": " << window_rect.x << ",\n";
    f << "  \"window_y\": " << window_rect.y << ",\n";
    f << "  \"window_w\": " << window_rect.w << ",\n";
    f << "  \"window_h\": " << window_rect.h << ",\n";
    f << "  \"font_size\": " << font_size << ",\n";
    f << "  \"recent_files\": [";
    for (size_t i = 0; i < recent_files_.size(); ++i) {
        if (i > 0) f << ", ";
        f << escape_json(recent_files_[i]);
    }
    f << "]\n}\n";
    return f.good();
}

// Simple extraction helpers (avoids JSON library dependency)
static int extract_int(const std::string& json, const std::string& key, int def) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return def;
    }
}

static float extract_float(const std::string& json, const std::string& key, float def) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        return std::stof(json.substr(pos));
    } catch (...) {
        return def;
    }
}

bool Settings::load() {
    std::string path = settings_path();
    if (path.empty()) return false;

    std::ifstream f(path);
    if (!f) return false;

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    window_rect.x = extract_int(json, "window_x", -1);
    window_rect.y = extract_int(json, "window_y", -1);
    window_rect.w = std::clamp(extract_int(json, "window_w", 1400), 200, 7680);
    window_rect.h = std::clamp(extract_int(json, "window_h", 900), 150, 4320);
    font_size = std::clamp(extract_float(json, "font_size", 16.0f), 8.0f, 48.0f);

    // Parse recent_files array
    recent_files_.clear();
    auto arr_pos = json.find("\"recent_files\"");
    if (arr_pos != std::string::npos) {
        auto bracket = json.find('[', arr_pos);
        auto end_bracket = json.find(']', bracket);
        if (bracket != std::string::npos && end_bracket != std::string::npos) {
            std::string arr = json.substr(bracket + 1, end_bracket - bracket - 1);
            size_t pos = 0;
            while (pos < arr.size()) {
                auto q1 = arr.find('"', pos);
                if (q1 == std::string::npos) break;
                auto q2 = q1 + 1;
                while (q2 < arr.size() && arr[q2] != '"') {
                    if (arr[q2] == '\\') ++q2;
                    ++q2;
                }
                if (q2 < arr.size()) {
                    std::string file;
                    for (size_t i = q1 + 1; i < q2; ++i) {
                        if (arr[i] == '\\' && i + 1 < q2) {
                            ++i;
                            if (arr[i] == 'n') file += '\n';
                            else file += arr[i];
                        } else {
                            file += arr[i];
                        }
                    }
                    if (!file.empty())
                        recent_files_.push_back(file);
                }
                pos = q2 + 1;
            }
        }
    }

    return true;
}

}  // namespace magic
