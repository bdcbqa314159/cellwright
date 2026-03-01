#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

namespace magic {

class PluginAllowlist {
public:
    PluginAllowlist();                                          // ~/.magic_dashboard/trusted_plugins.json
    explicit PluginAllowlist(std::filesystem::path json_path);  // testable

    static std::string sha256_of_file(const std::string& path);
    bool is_trusted(const std::string& path) const;
    void trust(const std::string& path);
    void revoke(const std::string& hash);
    static bool verify_codesign(const std::string& path);

private:
    void load();
    void save() const;

    std::filesystem::path json_path_;
    std::unordered_set<std::string> trusted_;
};

}  // namespace magic
