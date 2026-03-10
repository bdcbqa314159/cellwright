#include "plugin/PluginAllowlist.hpp"
#include "util/Sha256.hpp"

#include <fstream>
#include <iostream>

#ifdef __APPLE__
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace magic {

// ── Constructors ────────────────────────────────────────────────────────────

PluginAllowlist::PluginAllowlist() {
    const char* home = std::getenv("HOME");
    if (home && home[0] == '/') {
        json_path_ = std::filesystem::path(home) / ".cellwright" / "trusted_plugins.json";
    }
    load();
}

PluginAllowlist::PluginAllowlist(std::filesystem::path json_path)
    : json_path_(std::move(json_path)) {
    load();
}

// ── SHA-256 (forwards to free function) ─────────────────────────────────────

std::string PluginAllowlist::sha256_of_file(const std::string& path) {
    return magic::sha256_of_file(path);
}

// ── Trust queries ───────────────────────────────────────────────────────────

bool PluginAllowlist::is_trusted(const std::string& path) const {
    std::string hash = magic::sha256_of_file(path);
    if (hash.empty()) return false;
    return trusted_.count(hash) > 0;
}

void PluginAllowlist::trust(const std::string& path) {
    std::string hash = magic::sha256_of_file(path);
    if (hash.empty()) return;
    trusted_.insert(hash);
    save();
}

void PluginAllowlist::revoke(const std::string& hash) {
    trusted_.erase(hash);
    save();
}

// ── Codesign verification (macOS) ───────────────────────────────────────────

bool PluginAllowlist::verify_codesign(const std::string& path) {
#ifdef __APPLE__
    const char* argv[] = {"codesign", "-v", path.c_str(), nullptr};
    pid_t pid;
    int status;
    if (posix_spawn(&pid, "/usr/bin/codesign", nullptr, nullptr,
                    const_cast<char**>(argv), environ) != 0)
        return false;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    (void)path;
    return false;
#endif
}

// ── JSON persistence (hand-parsed, no nlohmann) ─────────────────────────────

void PluginAllowlist::load() {
    if (json_path_.empty()) return;
    std::ifstream file(json_path_);
    if (!file) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Parse a JSON array of strings: ["hash1", "hash2", ...]
    trusted_.clear();
    std::string::size_type pos = 0;
    while ((pos = content.find('"', pos)) != std::string::npos) {
        ++pos;  // skip opening quote
        auto end = content.find('"', pos);
        if (end == std::string::npos) break;
        trusted_.insert(content.substr(pos, end - pos));
        pos = end + 1;
    }
}

void PluginAllowlist::save() const {
    if (json_path_.empty()) return;

    std::filesystem::create_directories(json_path_.parent_path());

    std::ofstream file(json_path_);
    if (!file) {
        std::cerr << "[PluginAllowlist] Failed to write: " << json_path_ << "\n";
        return;
    }

    file << "[\n";
    bool first = true;
    for (const auto& hash : trusted_) {
        if (!first) file << ",\n";
        file << "  \"" << hash << "\"";
        first = false;
    }
    file << "\n]\n";
}

}  // namespace magic
