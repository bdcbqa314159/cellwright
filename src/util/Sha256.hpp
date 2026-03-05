#pragma once

#include <CommonCrypto/CommonDigest.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace magic {

// Compute SHA-256 hex digest of a file. Returns empty string on failure.
inline std::string sha256_of_file(const std::string& path) {
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

}  // namespace magic
