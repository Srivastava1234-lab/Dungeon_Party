// GameData.cpp (SCRUM-5)
// ----------------------
// Reads the JSON files in data/ with nlohmann/json and turns them into the structs
// from GameData.h. Every mistake in a file is reported with a GameDataError whose
// message names the file and the entry, for example:
//     cards.json, card "strike": "manaCost" must be 0 or more (found -1)
//
// What is checked here (data format only):
//   - the file exists and is valid JSON
//   - every required field is there and has the right type
//   - numbers are whole numbers inside their allowed range
//   - enum texts are known names from Types.h (e.g. "ATTACK", "WARRIOR")
//   - ids are unique and references point to existing entries
// Game rules (e.g. "heal cards only for the Cleric") are NOT checked here,
// because Kagan's classes already check them when the objects are created.

#include "GameData.h"

#include <climits>     // INT_MAX
#include <filesystem>  // std::filesystem::path (joins folder + file name on every OS)
#include <fstream>     // std::ifstream

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Helper functions that are only used inside this file.
// The empty "namespace { }" keeps them private to GameData.cpp.
namespace {

// ===================== Error helper =====================

// Throws a GameDataError with the message "<where>: <problem>".
// [[noreturn]] tells the compiler that this function never returns normally.
[[noreturn]] void fail(const std::string& where, const std::string& problem) {
    throw GameDataError(where + ": " + problem);
}

// ===================== Reading a file =====================

// Opens <dataDirectory>/<fileName>, parses it as JSON and checks that it is an object { ... }.
json readJsonFile(const std::string& dataDirectory, const std::string& fileName) {
    const std::filesystem::path fullPath = std::filesystem::path(dataDirectory) / fileName;

    std::ifstream file(fullPath);
    if (!file) {
        fail(fileName, "cannot open the file (looked at: " + fullPath.string() + ")");
    }

    json root;
    try {
        root = json::parse(file);
    } catch (const json::parse_error& error) {
        // error.what() contains the line and column of the mistake
        fail(fileName, std::string("not valid JSON - ") + error.what());
    }

    if (!root.is_object()) {
        fail(fileName, "the file must contain one JSON object { ... }");
    }
    return root;
}

// ===================== Reading single fields =====================

// Returns the field 'key' of 'object', or fails if the field is missing.
const json& requireField(const json& object, const std::string& key, const std::string& where) {
    if (!object.contains(key)) {
        fail(where, "missing field \"" + key + "\"");
    }
    return object.at(key);
}

// Fails if 'value' is not a JSON object { ... }. Used for the entries inside lists.
void requireObject(const json& value, const std::string& where) {
    if (!value.is_object()) {
        fail(where, "must be a JSON object { ... }");
    }
}

// Reads a text field that must not be empty.
std::string readText(const json& object, const std::string& key, const std::string& where) {
    const json& value = requireField(object, key, where);
    if (!value.is_string()) {
        fail(where, "\"" + key + "\" must be text in quotes");
    }
    std::string text = value.get<std::string>();
    if (text.empty()) {
        fail(where, "\"" + key + "\" must not be empty");
    }
    return text;
}

// Reads a whole number that must be at least 'minimum'.
int readNumber(const json& object, const std::string& key, int minimum, const std::string& where) {
    const json& value = requireField(object, key, where);

    // is_number_integer() is false for 3.5, for "3" (text) and for true/false.
    if (!value.is_number_integer()) {
        fail(where, "\"" + key + "\" must be a whole number");
    }

    // Reject numbers that are too big for an int BEFORE converting to int.
    // nlohmann can store a whole number in two ways: "unsigned" (positive numbers read
    // from a file) or "signed" (negative numbers, or numbers created in code).
    // Both kinds are checked, so nothing larger than INT_MAX can get through.
    bool tooLarge = false;
    if (value.is_number_unsigned()) {
        tooLarge = value.get<unsigned long long>() > static_cast<unsigned long long>(INT_MAX);
    } else {
        tooLarge = value.get<long long>() > static_cast<long long>(INT_MAX);
    }
    if (tooLarge) {
        fail(where, "\"" + key + "\" is too large (largest allowed: " + std::to_string(INT_MAX) + ")");
    }

    // Safe now: the value is at most INT_MAX, so it fits in a long long.
    // Values below 'minimum' (including very negative ones) are rejected here,
    // still before the conversion to int.
    const long long number = value.get<long long>();
    if (number < minimum) {
        fail(where, "\"" + key + "\" must be " + std::to_string(minimum) +
                    " or more (found " + std::to_string(number) + ")");
    }
    return static_cast<int>(number);
}

// Reads true or false.
bool readTrueFalse(const json& object, const std::string& key, const std::string& where) {
    const json& value = requireField(object, key, where);
    if (!value.is_boolean()) {
        fail(where, "\"" + key + "\" must be true or false (without quotes)");
    }
    return value.get<bool>();
}

// Reads a list [ ... ] that must contain at least one entry.
// Returns a copy of the list (the lists are small, and a copy is always safe to keep).
json readList(const json& object, const std::string& key, const std::string& where) {
    const json& value = requireField(object, key, where);
    if (!value.is_array()) {
        fail(where, "\"" + key + "\" must be a list [ ... ]");
    }
    if (value.empty()) {
        fail(where, "\"" + key + "\" must contain at least one entry");
    }
    return value;
}

// Reads the "id" field: not empty, only lowercase letters, digits and '_'.
std::string readId(const json& object, const std::string& where) {
    std::string id = readText(object, "id", where);
    for (char c : id) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (!allowed) {
            fail(where, "id \"" + id + "\" may only contain lowercase letters, digits and _");
        }
    }
    return id;
}

// ===================== Text -> enum from Types.h =====================
// The JSON files use the same CAPITAL names as the enums in Kagan's Types.h.

Side toSide(const std::string& text, const std::string& where) {
    if (text == "HEROES") return Side::HEROES;
    if (text == "DUNGEON_MASTER") return Side::DUNGEON_MASTER;
    fail(where, "unknown side \"" + text + "\" (allowed: HEROES, DUNGEON_MASTER)");
}

HeroEffect toHeroEffect(const std::string& text, const std::string& where) {
    if (text == "ATTACK") return HeroEffect::ATTACK;
    if (text == "HEAL") return HeroEffect::HEAL;
    if (text == "BUFF") return HeroEffect::BUFF;
    if (text == "SPELL") return HeroEffect::SPELL;
    fail(where, "unknown effect \"" + text + "\" for a HEROES card (allowed: ATTACK, HEAL, BUFF, SPELL)");
}

DMEffect toDMEffect(const std::string& text, const std::string& where) {
    if (text == "SUMMON") return DMEffect::SUMMON;
    if (text == "TRAP") return DMEffect::TRAP;
    if (text == "DEFENSIVE") return DMEffect::DEFENSIVE;
    fail(where, "unknown effect \"" + text + "\" for a DUNGEON_MASTER card (allowed: SUMMON, TRAP, DEFENSIVE)");
}

HeroClass toHeroClass(const std::string& text, const std::string& where) {
    if (text == "WARRIOR") return HeroClass::WARRIOR;
    if (text == "RANGER") return HeroClass::RANGER;
    if (text == "CLERIC") {
        // TODO(SCRUM-5): CLERIC vs HEALER.
        // The team agreed to call this class "CLERIC" (Vision document, UML OCL constraints,
        // UML persistence diagram), and the data files use "CLERIC".
        // Kagan's Types.h still has HeroClass::HEALER, and we do not change his code without
        // discussing it. So for now "CLERIC" is stored as HeroClass::HEALER.
        // When the enum is renamed to HeroClass::CLERIC, change this line to
        //     return HeroClass::CLERIC;
        // and delete this TODO (and the one in GameData.h).
        return HeroClass::HEALER;
    }
    if (text == "HEALER") {
        fail(where, "write \"CLERIC\" instead of \"HEALER\" in the data files (see data/README.md)");
    }
    fail(where, "unknown heroClass \"" + text + "\" (allowed: WARRIOR, RANGER, CLERIC)");
}

} // namespace

// ===================== GameData: loading =====================

GameData::GameData(const std::string& directory)
    : dataDirectory(directory) {
    loadCards();
    loadHeroes();   // must come after loadCards(): hero decks refer to cards
    loadMonsters();
}

std::string GameData::defaultDataDirectory() {
    // CMake can set DUNGEON_PARTY_DATA_DIR to the full path of the data/ folder.
#ifdef DUNGEON_PARTY_DATA_DIR
    return DUNGEON_PARTY_DATA_DIR;
#else
    return "data"; // relative to the folder the program is started from
#endif
}

void GameData::loadCards() {
    const std::string fileName = "cards.json";
    const json root = readJsonFile(dataDirectory, fileName);
    const json list = readList(root, "cards", fileName);

    for (std::size_t i = 0; i < list.size(); ++i) {
        const json& entry = list[i];
        // Before we know the id, we describe the entry by its position: card #1, #2, ...
        std::string where = fileName + ", card #" + std::to_string(i + 1);
        requireObject(entry, where);

        CardDefinition card;
        card.id = readId(entry, where);
        where = fileName + ", card \"" + card.id + "\"";
        if (findCard(card.id) != nullptr) {
            fail(where, "duplicate id (another card already uses \"" + card.id + "\")");
        }

        card.name = readText(entry, "name", where);
        card.side = toSide(readText(entry, "side", where), where);

        // HEROES cards use HeroEffect, DUNGEON_MASTER cards use DMEffect.
        const std::string effect = readText(entry, "effect", where);
        if (card.side == Side::HEROES) {
            card.heroEffect = toHeroEffect(effect, where);
        } else {
            card.dmEffect = toDMEffect(effect, where);
        }

        card.manaCost = readNumber(entry, "manaCost", 0, where);
        card.value = readNumber(entry, "value", 0, where);
        card.description = readText(entry, "description", where);
        card.temporary = readTrueFalse(entry, "temporary", where);

        cards.push_back(card);
    }
}

void GameData::loadHeroes() {
    const std::string fileName = "heroes.json";
    const json root = readJsonFile(dataDirectory, fileName);
    const json list = readList(root, "heroes", fileName);

    for (std::size_t i = 0; i < list.size(); ++i) {
        const json& entry = list[i];
        std::string where = fileName + ", hero #" + std::to_string(i + 1);
        requireObject(entry, where);

        HeroDefinition hero;
        hero.id = readId(entry, where);
        where = fileName + ", hero \"" + hero.id + "\"";
        if (findHero(hero.id) != nullptr) {
            fail(where, "duplicate id (another hero already uses \"" + hero.id + "\")");
        }

        hero.name = readText(entry, "name", where);
        hero.heroClass = toHeroClass(readText(entry, "heroClass", where), where);
        hero.maxHealth = readNumber(entry, "maxHealth", 1, where);
        hero.maxMana = readNumber(entry, "maxMana", 0, where);

        // Starting deck: every line must name an existing HEROES card.
        const json deck = readList(entry, "startingDeck", where);
        for (std::size_t j = 0; j < deck.size(); ++j) {
            const std::string deckWhere = where + ", startingDeck #" + std::to_string(j + 1);
            requireObject(deck[j], deckWhere);

            DeckEntry line;
            line.cardId = readText(deck[j], "card", deckWhere);
            line.count = readNumber(deck[j], "count", 1, deckWhere);

            const CardDefinition* card = findCard(line.cardId);
            if (card == nullptr) {
                fail(deckWhere, "unknown card \"" + line.cardId + "\" (not found in cards.json)");
            }
            if (card->side != Side::HEROES) {
                fail(deckWhere, "card \"" + line.cardId + "\" is a DUNGEON_MASTER card and cannot be in a hero deck");
            }
            hero.startingDeck.push_back(line);
        }

        hero.temporary = readTrueFalse(entry, "temporary", where);
        heroes.push_back(hero);
    }
}

void GameData::loadMonsters() {
    const std::string fileName = "monsters.json";
    const json root = readJsonFile(dataDirectory, fileName);
    const json list = readList(root, "monsters", fileName);

    for (std::size_t i = 0; i < list.size(); ++i) {
        const json& entry = list[i];
        std::string where = fileName + ", monster #" + std::to_string(i + 1);
        requireObject(entry, where);

        MonsterDefinition monster;
        monster.id = readId(entry, where);
        where = fileName + ", monster \"" + monster.id + "\"";
        if (findMonster(monster.id) != nullptr) {
            fail(where, "duplicate id (another monster already uses \"" + monster.id + "\")");
        }

        monster.name = readText(entry, "name", where);
        monster.maxHealth = readNumber(entry, "maxHealth", 1, where);
        monster.attackPower = readNumber(entry, "attackPower", 0, where);
        monster.boss = readTrueFalse(entry, "boss", where);
        monster.temporary = readTrueFalse(entry, "temporary", where);

        monsters.push_back(monster);
    }
}

LevelDefinition GameData::loadLevel(int number) const {
    if (number < 1) {
        throw GameDataError("Level number must be 1 or more (got " + std::to_string(number) + ")");
    }

    const std::string fileName = "levels/level_" + std::to_string(number) + ".json";
    const json root = readJsonFile(dataDirectory, fileName);

    LevelDefinition level;
    level.number = readNumber(root, "number", 1, fileName);
    if (level.number != number) {
        // Catches copy-paste mistakes, e.g. level_2.json still saying "number": 1
        fail(fileName, "\"number\" is " + std::to_string(level.number) +
                       " but the file name says level " + std::to_string(number));
    }

    level.name = readText(root, "name", fileName);
    level.maxRounds = readNumber(root, "maxRounds", 1, fileName);

    // defaultHeroes: a list of hero ids, e.g. ["warrior"]
    const json heroList = readList(root, "defaultHeroes", fileName);
    for (const json& heroId : heroList) {
        if (!heroId.is_string()) {
            fail(fileName, "\"defaultHeroes\" must only contain hero ids in quotes");
        }
        const std::string id = heroId.get<std::string>();
        if (findHero(id) == nullptr) {
            fail(fileName, "unknown hero \"" + id + "\" in defaultHeroes (not found in heroes.json)");
        }
        level.defaultHeroes.push_back(id);
    }

    // monsters: a list of { "monster": "<id>", "count": <number> }
    const json monsterList = readList(root, "monsters", fileName);
    for (std::size_t i = 0; i < monsterList.size(); ++i) {
        const std::string where = fileName + ", monsters #" + std::to_string(i + 1);
        requireObject(monsterList[i], where);

        LevelMonsterEntry line;
        line.monsterId = readText(monsterList[i], "monster", where);
        line.count = readNumber(monsterList[i], "count", 1, where);
        if (findMonster(line.monsterId) == nullptr) {
            fail(where, "unknown monster \"" + line.monsterId + "\" (not found in monsters.json)");
        }
        level.monsters.push_back(line);
    }

    level.temporary = readTrueFalse(root, "temporary", fileName);
    return level;
}

// ===================== GameData: looking things up =====================

const CardDefinition* GameData::findCard(const std::string& id) const {
    for (const CardDefinition& card : cards) {
        if (card.id == id) return &card;
    }
    return nullptr;
}

const HeroDefinition* GameData::findHero(const std::string& id) const {
    for (const HeroDefinition& hero : heroes) {
        if (hero.id == id) return &hero;
    }
    return nullptr;
}

const MonsterDefinition* GameData::findMonster(const std::string& id) const {
    for (const MonsterDefinition& monster : monsters) {
        if (monster.id == id) return &monster;
    }
    return nullptr;
}

const CardDefinition& GameData::getCard(const std::string& id) const {
    const CardDefinition* card = findCard(id);
    if (card == nullptr) throw GameDataError("Unknown card id \"" + id + "\"");
    return *card;
}

const HeroDefinition& GameData::getHero(const std::string& id) const {
    const HeroDefinition* hero = findHero(id);
    if (hero == nullptr) throw GameDataError("Unknown hero id \"" + id + "\"");
    return *hero;
}

const MonsterDefinition& GameData::getMonster(const std::string& id) const {
    const MonsterDefinition* monster = findMonster(id);
    if (monster == nullptr) throw GameDataError("Unknown monster id \"" + id + "\"");
    return *monster;
}

const std::vector<CardDefinition>& GameData::getCards() const { return cards; }
const std::vector<HeroDefinition>& GameData::getHeroes() const { return heroes; }
const std::vector<MonsterDefinition>& GameData::getMonsters() const { return monsters; }
const std::string& GameData::getDataDirectory() const { return dataDirectory; }
