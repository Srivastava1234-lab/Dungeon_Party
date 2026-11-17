#include "MatchResultStore.h"

#include <iostream>
#include <stdexcept>

namespace {

void check(bool passed, const char* message) {
    if (!passed) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
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
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
