#ifndef DUNGEON_PARTY_ACCOUNT_STORE_H
#define DUNGEON_PARTY_ACCOUNT_STORE_H

// AccountStore
// ------------
// Player accounts of one server: username + password -> a permanent, unique player ID
// ("P-K7RM2Q9X"). The ID never changes, so match results and a scoreboard can refer to it.
//
// Stored in a JSON file (e.g. accounts.json next to the server). Passwords are never stored,
// only a salted PBKDF2-SHA256 hash. An empty file path keeps the accounts in memory only (tests).

#include <map>
#include <optional>
#include <random>
#include <string>

class AccountStore {
public:
    struct Account {
        std::string playerId;   // permanent and unique, e.g. "P-K7RM2Q9X"
        std::string username;   // as the player typed it; unique ignoring upper/lower case
        std::string salt;       // hex
        std::string hash;       // hex, PBKDF2-SHA256(password, salt)
        int iterations = 0;
    };

    struct Result {
        bool ok = false;
        std::string error;      // shown to the player when !ok
        Account account;
    };

    explicit AccountStore(std::string filePath = "");

    Result registerAccount(const std::string& username, const std::string& password);
    Result login(const std::string& username, const std::string& password) const;

    std::size_t size() const { return accounts.size(); }

    // Rules, also used to give clear messages
    static std::optional<std::string> checkUsername(const std::string& username); // error text or nothing
    static std::optional<std::string> checkPassword(const std::string& password);

private:
    std::string filePath;
    std::map<std::string, Account> accounts; // key: lower-case username
    std::mt19937 rng;

    void load();
    void save() const;
    std::string newPlayerId();
    static std::string key(const std::string& username);
};

#endif
