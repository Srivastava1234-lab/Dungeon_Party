#include "AccountStore.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "../net/Protocol.h"
#include "PasswordHasher.h"

using nlohmann::json;

AccountStore::AccountStore(std::string path) : filePath(std::move(path)), rng(std::random_device{}()) {
    load();
}

std::string AccountStore::key(const std::string& username) {
    std::string k = username;
    std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return k;
}

std::optional<std::string> AccountStore::checkUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > net::MAX_NAME_LENGTH)
        return "Username must be 3 to " + std::to_string(net::MAX_NAME_LENGTH) + " characters";
    for (unsigned char c : username)
        if (!std::isalnum(c) && c != '_' && c != '-') return "Username may only use letters, digits, _ and -";
    return std::nullopt;
}

std::optional<std::string> AccountStore::checkPassword(const std::string& password) {
    if (password.size() < 6) return "Password must be at least 6 characters";
    if (password.size() > 64) return "Password must be at most 64 characters";
    return std::nullopt;
}

AccountStore::Result AccountStore::registerAccount(const std::string& username, const std::string& password) {
    Result result;
    if (auto problem = checkUsername(username)) { result.error = *problem; return result; }
    if (auto problem = checkPassword(password)) { result.error = *problem; return result; }
    if (accounts.count(key(username))) { result.error = "That username is already taken"; return result; }

    Account account;
    account.playerId = newPlayerId();
    account.username = username;
    const passwords::Bytes salt = passwords::randomBytes(16);
    account.salt = passwords::toHex(salt);
    account.iterations = passwords::DEFAULT_ITERATIONS;
    account.hash = passwords::toHex(passwords::pbkdf2Sha256(password, salt, account.iterations));
    accounts[key(username)] = account;
    save();

    result.ok = true;
    result.account = account;
    return result;
}

AccountStore::Result AccountStore::login(const std::string& username, const std::string& password) const {
    Result result;
    result.error = "Wrong username or password"; // same text for both, so names can't be probed
    auto it = accounts.find(key(username));
    if (it == accounts.end()) {
        // Do the same amount of work as a real check, so timing doesn't reveal unknown names.
        passwords::pbkdf2Sha256(password, passwords::Bytes(16, 0), passwords::DEFAULT_ITERATIONS);
        return result;
    }
    const Account& account = it->second;
    try {
        const passwords::Bytes expected = passwords::fromHex(account.hash);
        const passwords::Bytes actual =
            passwords::pbkdf2Sha256(password, passwords::fromHex(account.salt), account.iterations, expected.size());
        if (!passwords::equalConstantTime(expected, actual)) return result;
    } catch (const std::exception&) {
        return result; // damaged entry in the file
    }
    result.ok = true;
    result.error.clear();
    result.account = account;
    return result;
}

std::string AccountStore::newPlayerId() {
    std::uniform_int_distribution<std::size_t> pick(0, sizeof(net::ROOM_CODE_ALPHABET) - 2);
    while (true) {
        std::string id = "P-";
        for (int i = 0; i < 8; ++i) id += net::ROOM_CODE_ALPHABET[pick(rng)];
        const bool taken = std::any_of(accounts.begin(), accounts.end(),
                                       [&](const auto& entry) { return entry.second.playerId == id; });
        if (!taken) return id;
    }
}

void AccountStore::load() {
    if (filePath.empty() || !std::filesystem::exists(filePath)) return;
    std::ifstream in(filePath);
    const json root = json::parse(in, nullptr, false);
    if (root.is_discarded() || !root.contains("accounts") || !root["accounts"].is_array()) {
        std::cerr << "Accounts: " << filePath << " is not readable - starting with no accounts\n";
        return;
    }
    for (const auto& entry : root["accounts"]) {
        Account account;
        account.playerId = entry.value("playerId", "");
        account.username = entry.value("username", "");
        account.salt = entry.value("salt", "");
        account.hash = entry.value("hash", "");
        account.iterations = entry.value("iterations", 0);
        if (account.playerId.empty() || account.username.empty() || account.iterations < 1) continue;
        accounts[key(account.username)] = account;
    }
}

void AccountStore::save() const {
    if (filePath.empty()) return;
    json list = json::array();
    for (const auto& [k, a] : accounts)
        list.push_back({{"playerId", a.playerId}, {"username", a.username}, {"salt", a.salt},
                        {"hash", a.hash}, {"iterations", a.iterations}});
    const json root = {{"_note", "Dungeon Party accounts. Passwords are not stored, only salted PBKDF2-SHA256 hashes. Do not commit this file."},
                       {"accounts", list}};
    // Write to a temporary file first, then replace: a crash can't leave a half-written file.
    const std::string temporary = filePath + ".tmp";
    {
        std::ofstream out(temporary);
        out << root.dump(2) << "\n";
    }
    std::error_code error;
    std::filesystem::rename(temporary, filePath, error);
    if (error) { // some Windows compilers can't rename over an existing file
        std::filesystem::remove(filePath, error);
        error.clear();
        std::filesystem::rename(temporary, filePath, error);
    }
    if (error) std::cerr << "Accounts: could not save " << filePath << ": " << error.message() << "\n";
}
