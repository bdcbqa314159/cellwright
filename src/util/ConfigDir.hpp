#pragma once
#include <cstdlib>
#include <string>

namespace magic {

inline std::string get_config_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) return appdata;
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) return userprofile;
#endif
    const char* home = std::getenv("HOME");
    return home ? home : ".";
}

}  // namespace magic
