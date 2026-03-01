#include <gtest/gtest.h>
#include "plugin/PluginAllowlist.hpp"

#include <filesystem>
#include <fstream>

using namespace magic;
namespace fs = std::filesystem;

// ── Helpers ─────────────────────────────────────────────────────────────────

class PluginAllowlistTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "allowlist_test";
        fs::create_directories(tmp_dir_);
        json_path_ = tmp_dir_ / "trusted_plugins.json";
    }

    void TearDown() override {
        fs::remove_all(tmp_dir_);
    }

    // Write a file with known content and return its path
    std::string write_tmp_file(const std::string& name, const std::string& content) {
        auto p = tmp_dir_ / name;
        std::ofstream f(p, std::ios::binary);
        f << content;
        return p.string();
    }

    fs::path tmp_dir_;
    fs::path json_path_;
};

// ── SHA-256 tests ───────────────────────────────────────────────────────────

TEST_F(PluginAllowlistTest, Sha256KnownContent) {
    // SHA-256("hello world") is well-known
    auto path = write_tmp_file("hello.txt", "hello world");
    auto hash = PluginAllowlist::sha256_of_file(path);
    EXPECT_EQ(hash, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

TEST_F(PluginAllowlistTest, Sha256NonexistentFile) {
    auto hash = PluginAllowlist::sha256_of_file("/nonexistent/file.bin");
    EXPECT_TRUE(hash.empty());
}

TEST_F(PluginAllowlistTest, Sha256EmptyFile) {
    auto path = write_tmp_file("empty.bin", "");
    auto hash = PluginAllowlist::sha256_of_file(path);
    // SHA-256 of empty input
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// ── Trust / is_trusted ──────────────────────────────────────────────────────

TEST_F(PluginAllowlistTest, TrustThenIsTrusted) {
    PluginAllowlist al(json_path_);
    auto path = write_tmp_file("plugin.dylib", "plugin binary content");

    EXPECT_FALSE(al.is_trusted(path));
    al.trust(path);
    EXPECT_TRUE(al.is_trusted(path));
}

TEST_F(PluginAllowlistTest, UntrustedByDefault) {
    PluginAllowlist al(json_path_);
    auto path = write_tmp_file("rogue.dylib", "malicious content");
    EXPECT_FALSE(al.is_trusted(path));
}

// ── Revoke ──────────────────────────────────────────────────────────────────

TEST_F(PluginAllowlistTest, RevokeRemovesTrust) {
    PluginAllowlist al(json_path_);
    auto path = write_tmp_file("plugin.dylib", "plugin binary content");

    al.trust(path);
    EXPECT_TRUE(al.is_trusted(path));

    auto hash = PluginAllowlist::sha256_of_file(path);
    al.revoke(hash);
    EXPECT_FALSE(al.is_trusted(path));
}

// ── Persistence round-trip ──────────────────────────────────────────────────

TEST_F(PluginAllowlistTest, PersistenceRoundTrip) {
    auto path = write_tmp_file("plugin.dylib", "persistent content");

    {
        PluginAllowlist al(json_path_);
        al.trust(path);
        EXPECT_TRUE(al.is_trusted(path));
    }

    // Construct a fresh allowlist from the same JSON — trust should survive
    PluginAllowlist al2(json_path_);
    EXPECT_TRUE(al2.is_trusted(path));
}

// ── verify_codesign ─────────────────────────────────────────────────────────

TEST_F(PluginAllowlistTest, CodesignUnsignedFile) {
    auto path = write_tmp_file("unsigned.dylib", "not a real dylib");
    EXPECT_FALSE(PluginAllowlist::verify_codesign(path));
}
