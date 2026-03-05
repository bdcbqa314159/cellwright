#include "plugin/PluginAllowlist.hpp"

#include <CommonCrypto/CommonDigest.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace magic {

// ── Constructors ────────────────────────────────────────────────────────────

PluginAllowlist::PluginAllowlist() {
    const char* home = std::getenv("HOME");
    if (home) {
        json_path_ = std::filesystem::path(home) / ".magic_dashboard" / "trusted_plugins.json";
    }
    load();
}

PluginAllowlist::PluginAllowlist(std::filesystem::path json_path)
    : json_path_(std::move(json_path)) {
    load();
}

// ── SHA-256 ─────────────────────────────────────────────────────────────────

std::string PluginAllowlist::sha256_of_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};

    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);

    char buf[8192];
    while (file.read(buf, sizeof(buf))) {
        CC_SHA256_Update(&ctx, buf, static_cast<CC_LONG>(file.gcount()));
    }
    // Final partial read
    if (file.gcount() > 0) {
        CC_SHA256_Update(&ctx, buf, static_cast<CC_LONG>(file.gcount()));
    }

    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &ctx);

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) {
        hex << std::setw(2) << static_cast<unsigned>(digest[i]);
    }
    return hex.str();
}

// ── Trust queries ───────────────────────────────────────────────────────────

bool PluginAllowlist::is_trusted(const std::string& path) const {
    std::string hash = sha256_of_file(path);
    if (hash.empty()) return false;
    return trusted_.count(hash) > 0;
}

void PluginAllowlist::trust(const std::string& path) {
    std::string hash = sha256_of_file(path);
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
    // Reject paths with shell metacharacters to prevent command injection
    for (char ch : path) {
        if (ch == '\'' || ch == '"' || ch == '\\' || ch == '$' || ch == '`'
            || ch == '(' || ch == ')' || ch == ';' || ch == '&' || ch == '|'
            || ch == '\n' || ch == '\r')
            return false;
    }
    std::string cmd = "codesign -v '" + path + "' 2>/dev/null";
    int status = std::system(cmd.c_str());
    return status == 0;
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
