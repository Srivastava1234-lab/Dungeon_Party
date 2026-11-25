#ifndef DUNGEON_PARTY_PASSWORD_HASHER_H
#define DUNGEON_PARTY_PASSWORD_HASHER_H

// Password hashing for player accounts: PBKDF2-HMAC-SHA256 with a random salt per account.
// Passwords are never stored - only salt + hash. Self-contained (the project has no crypto
// library); verified against the official SHA-256 / PBKDF2 test vectors in tests/account_tests.cpp.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace passwords {

using Bytes = std::vector<std::uint8_t>;

constexpr int DEFAULT_ITERATIONS = 20000; // slows down guessing; ~10 ms per login on the server

std::array<std::uint8_t, 32> sha256(const Bytes& data);
Bytes hmacSha256(const Bytes& key, const Bytes& message);
Bytes pbkdf2Sha256(const std::string& password, const Bytes& salt, int iterations, std::size_t length = 32);

Bytes randomBytes(std::size_t count);
std::string toHex(const Bytes& bytes);
Bytes fromHex(const std::string& hex);

// Compares in constant time, so the time taken doesn't reveal how much of a hash matched.
bool equalConstantTime(const Bytes& a, const Bytes& b);

} // namespace passwords

#endif
