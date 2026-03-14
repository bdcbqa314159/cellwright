#pragma once

#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef __APPLE__

#include <CommonCrypto/CommonDigest.h>

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

#else

// Portable SHA-256 implementation (public domain algorithm, RFC 6234)

#include <array>
#include <cstdint>
#include <cstring>

namespace magic {
namespace detail {

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

static constexpr uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

struct Sha256State {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t buffer[64] = {};
    uint64_t total_len = 0;
    size_t buf_len = 0;

    void process_block(const uint8_t* block) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
                   (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
        for (int i = 16; i < 64; ++i)
            w[i] = gamma1(w[i-2]) + w[i-7] + gamma0(w[i-15]) + w[i-16];

        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = hh + sigma1(e) + ch(e,f,g) + K[i] + w[i];
            uint32_t t2 = sigma0(a) + maj(a,b,c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    void update(const uint8_t* data, size_t len) {
        total_len += len;
        while (len > 0) {
            size_t space = 64 - buf_len;
            size_t copy = (len < space) ? len : space;
            std::memcpy(buffer + buf_len, data, copy);
            buf_len += copy;
            data += copy;
            len -= copy;
            if (buf_len == 64) {
                process_block(buffer);
                buf_len = 0;
            }
        }
    }

    std::array<uint8_t, 32> finalize() {
        uint64_t bits = total_len * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        pad = 0;
        while (buf_len != 56) update(&pad, 1);
        uint8_t len_bytes[8];
        for (int i = 7; i >= 0; --i) { len_bytes[i] = uint8_t(bits); bits >>= 8; }
        update(len_bytes, 8);

        std::array<uint8_t, 32> digest;
        for (int i = 0; i < 8; ++i) {
            digest[i*4+0] = uint8_t(h[i] >> 24);
            digest[i*4+1] = uint8_t(h[i] >> 16);
            digest[i*4+2] = uint8_t(h[i] >> 8);
            digest[i*4+3] = uint8_t(h[i]);
        }
        return digest;
    }
};

}  // namespace detail

inline std::string sha256_of_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};

    detail::Sha256State state;
    char buf[8192];
    while (file.read(buf, sizeof(buf))) {
        state.update(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(file.gcount()));
    }
    if (file.gcount() > 0) {
        state.update(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(file.gcount()));
    }

    auto digest = state.finalize();
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        hex << std::setw(2) << static_cast<unsigned>(digest[i]);
    }
    return hex.str();
}

}  // namespace magic

#endif
