// Player accounts: password hashing (official test vectors), saving and loading, file contents.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "AccountStore.h"
#include "PasswordHasher.h"

namespace {
void check(bool condition, const std::string& what) {
    if (!condition) { std::cerr << "FAILED: " << what << "\n"; std::exit(1); }
    std::cout << "  ok  " << what << "\n";
}
std::string hexOf(const std::array<std::uint8_t, 32>& h) { return passwords::toHex(passwords::Bytes(h.begin(), h.end())); }
}

int main() {
    using namespace passwords;
    // Official test vectors (FIPS 180-2 for SHA-256, RFC 7914 / RFC 6070-style for PBKDF2-HMAC-SHA256)
    check(hexOf(sha256(Bytes{'a', 'b', 'c'})) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256(\"abc\")");
    check(hexOf(sha256(Bytes{})) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256(\"\")");
    const Bytes salt{'s', 'a', 'l', 't'};
    check(toHex(pbkdf2Sha256("password", salt, 1)) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", "PBKDF2, 1 iteration");
    check(toHex(pbkdf2Sha256("password", salt, 2)) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43", "PBKDF2, 2 iterations");
    check(toHex(pbkdf2Sha256("password", salt, 4096)) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a", "PBKDF2, 4096 iterations");

    const std::string file = (std::filesystem::temp_directory_path() / "dungeon_party_account_test.json").string();
    std::filesystem::remove(file);
    std::string kagansId;
    {
        AccountStore store(file);
        check(!store.registerAccount("x", "secret123").ok, "username too short is refused");
        check(!store.registerAccount("bad name!", "secret123").ok, "username with spaces/symbols is refused");
        check(!store.registerAccount("kagan", "123").ok, "password too short is refused");
        auto created = store.registerAccount("Kagan", "dragon-slayer");
        check(created.ok && created.account.playerId.rfind("P-", 0) == 0, "account created with a player ID");
        kagansId = created.account.playerId;
        check(!store.registerAccount("KAGAN", "something").ok, "the same name with other capitals is taken");
        auto other = store.registerAccount("Soroush", "dragon-slayer"); // same password as Kagan
        check(other.ok && other.account.playerId != kagansId, "every account gets a different player ID");
        check(other.account.salt != created.account.salt && other.account.hash != created.account.hash,
              "the same password gives a different hash (each account has its own salt)");
    }
    {
        AccountStore reloaded(file); // like restarting the server
        check(reloaded.size() == 2, "accounts are loaded again after a restart");
        auto login = reloaded.login("kagan", "dragon-slayer");
        check(login.ok && login.account.playerId == kagansId, "after a restart the player keeps the same ID");
        check(!reloaded.login("kagan", "dragon-slayer!").ok, "a wrong password is refused after a restart");
        check(!reloaded.login("nobody", "dragon-slayer").ok, "an unknown name is refused");
    }
    std::stringstream contents;
    contents << std::ifstream(file).rdbuf();
    check(contents.str().find("dragon-slayer") == std::string::npos, "the accounts file never contains a password");
    std::filesystem::remove(file);

    std::cout << "All account tests passed.\n";
    return 0;
}
