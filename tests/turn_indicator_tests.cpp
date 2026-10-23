#include "TurnIndicator.h"
#include <iostream>
#include <stdexcept>

void expect(const Match& match, const std::string& expected) {
    const auto actual = turnIndicatorText(match);
    if (actual != expected) throw std::runtime_error("Expected: " + expected + "; got: " + actual);
}
int main() {
    Match match("indicator-test", 2);
    expect(match, "Waiting for match to start");
    match.addPlayer(std::make_unique<Hero>("h1", "Hero 1", HeroType{HeroClass::WARRIOR, 30}, 3));
    match.addPlayer(std::make_unique<Hero>("h2", "Hero 2", HeroType{HeroClass::WARRIOR, 30}, 3));
    match.addPlayer(std::make_unique<DungeonMaster>("dm", "DM", 3));
    match.addMonster(std::make_unique<Monster>("Goblin", 10));
    match.start();
    expect(match, "Round 1 | Heroes' turn");
    match.endTurn(*match.getActivePlayer());
    expect(match, "Round 1 | Heroes' turn");
    match.endTurn(*match.getActivePlayer());
    expect(match, "Round 1 | Dungeon Master's turn");
    match.endTurn(*match.getActivePlayer());
    expect(match, "Round 2 | Heroes' turn");
    match.endTurn(*match.getActivePlayer());
    match.endTurn(*match.getActivePlayer());
    expect(match, "Round 2 | Dungeon Master's turn");
    match.endTurn(*match.getActivePlayer());
    expect(match, "Round 2 | Dungeon Master wins!");
    std::cout << "Turn indicator transitions passed\n";
}