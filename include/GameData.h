#ifndef DUNGEON_PARTY_GAMEDATA_H
#define DUNGEON_PARTY_GAMEDATA_H

// GameData (SCRUM-5)
// ------------------
// Loads the game data from the data/ folder: cards, heroes, monsters and levels.
// The JSON format of every file is described in data/README.md.
//
// This header only DESCRIBES the data. It does not create Card, Hero or Monster objects:
//   - Taegan's card/deck model turns a CardDefinition into a HeroCard or DMCard.
//   - Kagan's match setup turns HeroDefinition, MonsterDefinition and LevelDefinition
//     into Hero, Monster and Match objects.
//
// The JSON library (nlohmann/json) is only used inside GameData.cpp, so files that
// include this header do not need to know about JSON at all.

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Types.h" // Kagan's enums: Side, HeroEffect, DMEffect, HeroClass and struct HeroType

// Thrown when a data file is missing, is not valid JSON, or contains a wrong value.
// The message always names the file and the entry that has the problem.
class GameDataError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error; // reuse the std::runtime_error(message) constructor
};

// ===================== cards.json =====================

struct CardDefinition {
    std::string id;            // e.g. "strike"  - decks refer to the card by this id
    std::string name;          // e.g. "Strike"  - shown in the game
    Side side = Side::HEROES;  // HEROES -> HeroCard, DUNGEON_MASTER -> DMCard

    // Exactly ONE of these two is set, depending on 'side':
    std::optional<HeroEffect> heroEffect; // set when side == Side::HEROES
    std::optional<DMEffect> dmEffect;     // set when side == Side::DUNGEON_MASTER

    int manaCost = 0;          // mana needed to play the card
    int value = 0;             // strength of the effect (damage, healing, ...)
    std::string description;   // short text for the HUD (without numbers)
    bool temporary = true;     // true = values not confirmed by Irma yet
};

// ===================== heroes.json =====================

// One line of a starting deck, e.g. { "card": "strike", "count": 10 }
struct DeckEntry {
    std::string cardId;  // id of a HEROES card in cards.json
    int count = 0;       // number of copies of this card
};

struct HeroDefinition {
    std::string id;      // e.g. "warrior" - levels refer to the hero by this id
    std::string name;    // e.g. "Warrior" - shown in the game

    // TODO(SCRUM-5): CLERIC vs HEALER.
    // The team agreed on "CLERIC" (Vision document, UML constraints, UML persistence diagram),
    // but Kagan's Types.h still calls this class HeroClass::HEALER.
    // Until the enum is renamed, "CLERIC" in heroes.json is stored here as HeroClass::HEALER.
    // The translation happens in GameData.cpp. Do not write "HEALER" in the data files.
    HeroClass heroClass = HeroClass::WARRIOR;

    int maxHealth = 0;   // health at the start of the match
    int maxMana = 0;     // mana refilled every round
    std::vector<DeckEntry> startingDeck;
    bool temporary = true;

    // Kagan's Hero constructor expects a HeroType { heroClass, maxHealth }.
    HeroType toHeroType() const { return HeroType{heroClass, maxHealth}; }
};

// ===================== monsters.json =====================

struct MonsterDefinition {
    std::string id;       // e.g. "goblin"
    std::string name;     // e.g. "Goblin"
    int maxHealth = 0;    // health at the start of the match
    int attackPower = 0;  // UML Monster.attackPower (not a member of Kagan's Monster class yet)
    bool boss = false;    // UML Monster.boss (Boss/Monster naming is still open in the team)
    bool temporary = true;
};

// ===================== levels/level_N.json =====================

// One line of a level's monster list, e.g. { "monster": "goblin", "count": 1 }
struct LevelMonsterEntry {
    std::string monsterId; // id in monsters.json
    int count = 0;         // how many of this monster appear
};

struct LevelDefinition {
    int number = 0;                         // 1 for levels/level_1.json
    std::string name;                       // shown in the game
    int maxRounds = 0;                      // round limit (Kagan's Match::maxRounds)
    std::vector<std::string> defaultHeroes; // hero ids, only used until role selection exists (Sprint 2)
    std::vector<LevelMonsterEntry> monsters;
    bool temporary = true;
};

// ===================== The loader =====================

class GameData {
public:
    // Loads cards.json, heroes.json and monsters.json from 'dataDirectory' and checks them.
    // Throws GameDataError if a file is missing or contains a mistake.
    explicit GameData(const std::string& dataDirectory);

    // The data folder set by CMake (DUNGEON_PARTY_DATA_DIR), or "data" if CMake did not set it.
    static std::string defaultDataDirectory();

    // Loads levels/level_<number>.json and checks that its heroes and monsters exist.
    // Throws GameDataError if the file is missing or contains a mistake.
    LevelDefinition loadLevel(int number) const;

    // Find one entry by its id. Throw GameDataError if the id does not exist.
    const CardDefinition& getCard(const std::string& id) const;
    const HeroDefinition& getHero(const std::string& id) const;
    const MonsterDefinition& getMonster(const std::string& id) const;

    // All entries, in the same order as in the files.
    const std::vector<CardDefinition>& getCards() const;
    const std::vector<HeroDefinition>& getHeroes() const;
    const std::vector<MonsterDefinition>& getMonsters() const;

    const std::string& getDataDirectory() const;

private:
    std::string dataDirectory;
    std::vector<CardDefinition> cards;
    std::vector<HeroDefinition> heroes;
    std::vector<MonsterDefinition> monsters;

    // Called by the constructor in this order (heroes refer to cards, so cards come first).
    void loadCards();
    void loadHeroes();
    void loadMonsters();

    // Like getCard/getHero/getMonster, but return nullptr instead of throwing.
    // Used to check references (deck -> card, level -> hero/monster).
    const CardDefinition* findCard(const std::string& id) const;
    const HeroDefinition* findHero(const std::string& id) const;
    const MonsterDefinition* findMonster(const std::string& id) const;
};

#endif // DUNGEON_PARTY_GAMEDATA_H
