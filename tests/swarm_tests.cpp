// Swarms: monsters enter in groups; the Dungeon Master's boss only after the last swarm.
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Match.h"
#include "Warrior.h"

namespace {
void check(bool condition, const std::string& what) {
    if (!condition) { std::cerr << "FAILED: " << what << "\n"; std::exit(1); }
    std::cout << "  ok  " << what << "\n";
}

std::vector<std::unique_ptr<Monster>> group(std::initializer_list<const char*> names) {
    std::vector<std::unique_ptr<Monster>> monsters;
    for (const char* name : names) monsters.push_back(std::make_unique<Monster>(name, 30));
    return monsters;
}

void defeatAllMonsters(Match& match) {
    for (const auto& monster : match.getMonsters())
        if (monster->isAlive()) monster->takeDamage(1000);
}

// One full round: hero ends the turn, the DM ends the turn -> monster phase -> checks the battlefield.
void playRound(Match& match, Hero& hero, DungeonMaster& dm) {
    match.endTurn(hero);
    if (!match.isFinished()) match.endTurn(dm);
}

int countAlive(const Match& match) {
    int alive = 0;
    for (const auto& monster : match.getMonsters()) alive += monster->isAlive() ? 1 : 0;
    return alive;
}
}

int main() {
    Match match("swarms", 20);
    auto heroOwner = std::make_unique<Warrior>("h", "Hero", 100, 10);
    Hero& hero = *heroOwner;
    auto dmOwner = std::make_unique<DungeonMaster>("dm", "DM", 10, true);
    DungeonMaster& dm = *dmOwner;
    match.addPlayer(std::move(heroOwner));
    match.addPlayer(std::move(dmOwner));

    // Swarm 1 has two different kinds of monster in the same swarm.
    match.addSwarm("Raiders", group({"Goblin", "Skeleton"}));
    match.addSwarm("Undead", group({"Skeleton", "Lich"}));
    match.configureBoss(std::make_unique<Monster>("Final Boss", 80, true), 0, 20); // 0 = after the swarms
    match.start();

    check(match.getSwarmCount() == 2 && match.getSwarmNumber() == 1, "the match starts with swarm 1 of 2");
    check(match.getMonsters().size() == 2 && match.getMonsters()[0]->getName() == "Goblin" &&
          match.getMonsters()[1]->getName() == "Skeleton", "one swarm can contain different kinds of monster");
    check(!match.hasBossArrived(), "the boss is not on the battlefield at the start");

    // Rounds pass without the boss arriving: arrival depends on the swarms, not the round number.
    playRound(match, hero, dm);
    playRound(match, hero, dm);
    check(!match.hasBossArrived() && match.getSwarmNumber() == 1, "the boss does not arrive after a fixed number of rounds");

    defeatAllMonsters(match);
    playRound(match, hero, dm);
    check(!match.isFinished(), "defeating swarm 1 does not win the match");
    check(match.getSwarmNumber() == 2 && countAlive(match) == 2, "swarm 2 enters after swarm 1 is defeated");
    check(!match.hasBossArrived(), "the boss still waits while swarm 2 is alive");

    defeatAllMonsters(match);
    playRound(match, hero, dm);
    check(!match.isFinished(), "defeating the last swarm does not win the match");
    check(match.hasBossArrived() && countAlive(match) == 1, "the Dungeon Master's boss enters after both swarms");

    defeatAllMonsters(match);
    playRound(match, hero, dm);
    check(match.isFinished() && match.getWinner() == Side::HEROES, "the heroes win after the boss is defeated");

    std::cout << "All swarm tests passed.\n";
    return 0;
}
