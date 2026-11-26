#include "MatchResultStore.h"

#include <filesystem>
#include <random>
#include <iostream>
#include <stdexcept>

namespace {

void check(bool passed, const char* message) {
    if (!passed) throw std::runtime_error(message);
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
                           ("dungeon-results-" + std::to_string(std::random_device{}()));
    const auto database = directory / "results.sqlite";
    try {
        check(std::filesystem::create_directory(directory), "Could not create isolated test directory");
        MatchResultStore store(":memory:");
        store.save("match-1", Side::HEROES, {{"p2", "DM", "dm"}, {"p1", "First", "warrior"}});
        store.save("match-2", Side::DUNGEON_MASTER, {{"p1", "Second", "healer"}});
        store.save("match-3", Side::HEROES, {{"p1", "Third", "warrior"}});

        const auto first = store.load("match-1");
        check(first && first->matchId == "match-1" && first->winner == Side::HEROES &&
                  !first->finishedAtUtc.empty() && first->players.size() == 2 &&
                  first->players[0].id == "p1" && first->players[0].name == "First" &&
                  first->players[0].role == "warrior" && first->players[1].role == "dm",
              "First match must include its winner, timestamp and both players");

        const auto second = store.load("match-2");
        check(second && second->matchId == "match-2" && second->winner == Side::DUNGEON_MASTER &&
                  second->players.size() == 1 && second->players[0].name == "Second" &&
                  second->players[0].role == "healer",
              "Reused player IDs must not select records from another match");

        const auto third = store.load("match-3");
        check(third && third->matchId == "match-3" && third->players.size() == 1 &&
                  third->players[0].name == "Third", "Third match must load independently");
        check(!store.load("missing"), "Unknown match ID must return no result");
        bool rejected = false;
        try {
            store.save("match-1", Side::DUNGEON_MASTER, {{"p1", "Wrong", "dm"}});
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected && store.load("match-1")->winner == Side::HEROES,
              "A duplicate ID must not replace the original result");

        rejected = false;
        try {
            store.save("failed", Side::HEROES, {{"p1", "A", "warrior"}, {"p1", "B", "healer"}});
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected && !store.load("failed"), "An invalid player record must roll back the whole match");
        store.save("failed", Side::HEROES, {{"p1", "Recovered", "warrior"}});
        check(store.load("failed")->players[0].name == "Recovered",
              "A failed save must leave the connection ready for a retry");

        {
            MatchResultStore disk(database.string());
            disk.save("persisted", Side::HEROES, {{"p1", "Saved player", "warrior"}});
        }
        {
            MatchResultStore reopened(database.string());
            const auto saved = reopened.load("persisted");
            check(saved && saved->winner == Side::HEROES && saved->players.size() == 1 &&
                      saved->players[0].name == "Saved player",
                  "Completed results must survive closing and reopening the database");
        }
        std::filesystem::remove(database);
        std::filesystem::remove(directory);
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove(database, ignored);
        std::filesystem::remove(directory, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
