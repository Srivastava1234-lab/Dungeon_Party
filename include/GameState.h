#ifndef DUNGEON_PARTY_GAME_STATE_H
#define DUNGEON_PARTY_GAME_STATE_H

#include <optional>
#include <string>
#include <vector>
#include "Types.h"

class Match;
class Player;
class DungeonMaster;

// Scrum 29: What every player sees. Display-only copy of the Match, based on the rules agreed upon so far.
// The structures in here should match the JSON file produced by the serverside, in order to rebuild the last state of each
// character from the snapshots.
struct HeroState {
    std::string id;
    std::string name;
    HeroClass heroClass;
    int health;
    int maxHealth;
    int mana;
    int maxMana;
    int shield;
    int handSize;     // only the number of cards, never the cards of other players
    bool alive;
};

struct MonsterState {
    std::string name;
    int health;
    int maxHealth;
    bool boss;
    bool alive;
    int burnTurns;
};

struct CardState {
    std::string name;
    int manaCost;
    HeroEffect effect;
    int value;
};

struct GameState {
    int round = 0;
    Phase phase = Phase::ROUND_START;
    std::string activePlayerId;      // whose turn it is
    std::optional<Side> winner;      // empty while the match is running
    std::vector<HeroState> heroes;
    std::vector<MonsterState> monsters;
    int dmMana = 0;
    int dmMaxMana = 0;
    int dmHandSize = 0;
    std::vector<CardState> myHand;   // private: only the viewer's own cards
};

// Builds the snapshot of the match for one viewer(player) (nullptr = no hand).
GameState makeGameState(const Match& match, const DungeonMaster& dm, const Player* viewer);

// Message text <-> GameState.
std::string gameStateToJson(const GameState& state);
GameState gameStateFromJson(const std::string& text);   // throws on invalid text

#endif