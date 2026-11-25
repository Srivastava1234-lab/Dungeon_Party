#include "PasswordHasher.h"

#include <random>
#include <stdexcept>

namespace passwords {
namespace {

constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

} // namespace

std::array<std::uint8_t, 32> sha256(const Bytes& data) {
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    Bytes msg = data;
    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i) msg.push_back(static_cast<std::uint8_t>(bitLength >> (i * 8)));

    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (std::uint32_t(msg[chunk + 4 * i]) << 24) | (std::uint32_t(msg[chunk + 4 * i + 1]) << 16) |
                   (std::uint32_t(msg[chunk + 4 * i + 2]) << 8) | std::uint32_t(msg[chunk + 4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::array<std::uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 4; ++j) out[4 * i + j] = static_cast<std::uint8_t>(h[i] >> (24 - 8 * j));
    return out;
}

Bytes hmacSha256(const Bytes& key, const Bytes& message) {
    Bytes k = key;
    if (k.size() > 64) { auto hashed = sha256(k); k.assign(hashed.begin(), hashed.end()); }
    k.resize(64, 0);
    Bytes inner(64), outer(64);
    for (int i = 0; i < 64; ++i) { inner[i] = k[i] ^ 0x36; outer[i] = k[i] ^ 0x5c; }
    inner.insert(inner.end(), message.begin(), message.end());
    const auto innerHash = sha256(inner);
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());
    const auto result = sha256(outer);
    return Bytes(result.begin(), result.end());
}

Bytes pbkdf2Sha256(const std::string& password, const Bytes& salt, int iterations, std::size_t length) {
    if (iterations < 1) throw std::invalid_argument("iterations must be at least 1");
    const Bytes key(password.begin(), password.end());
    Bytes out;
    for (std::uint32_t block = 1; out.size() < length; ++block) {
        Bytes saltBlock = salt;
        for (int i = 3; i >= 0; --i) saltBlock.push_back(static_cast<std::uint8_t>(block >> (i * 8)));
        Bytes u = hmacSha256(key, saltBlock);
        Bytes t = u;
        for (int i = 1; i < iterations; ++i) {
            u = hmacSha256(key, u);
            for (std::size_t j = 0; j < t.size(); ++j) t[j] ^= u[j];
        }
        out.insert(out.end(), t.begin(), t.end());
    }
    out.resize(length);
    return out;
}

Bytes randomBytes(std::size_t count) {
    std::random_device device; // OS randomness
    Bytes out(count);
    for (auto& b : out) b = static_cast<std::uint8_t>(device() & 0xff);
    return out;
}

std::string toHex(const Bytes& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string hex;
    for (auto b : bytes) { hex += digits[b >> 4]; hex += digits[b & 0xf]; }
    return hex;
}

Bytes fromHex(const std::string& hex) {
    auto value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("not a hex digit");
    };
    if (hex.size() % 2) throw std::invalid_argument("odd hex length");
    Bytes out;
    for (std::size_t i = 0; i < hex.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(value(hex[i]) * 16 + value(hex[i + 1])));
    return out;
}

bool equalConstantTime(const Bytes& a, const Bytes& b) {
    if (a.size() != b.size()) return false;
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

} // namespace passwords
