// validate_data.cpp (SCRUM-5)
// ---------------------------
// A small command-line program that checks the files in data/.
// Run it after every change to a data file (for example after Irma updates values).
//
// Usage:
//   validate_data                 checks the data folder that CMake points to
//   validate_data <dataFolder>    checks another data folder
//
// Exit code:
//   0 = the data is OK (temporary values are only warnings)
//   1 = the data has an error (the message says which file and which entry)
//
// This program only uses GameData.h / GameData.cpp. It does not need the game window,
// OpenGL or any of the game logic classes, so it builds quickly.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "GameData.h"

namespace {

// ===================== Enum -> text (only for printing) =====================

std::string sideName(Side side) {
    switch (side) {
        case Side::HEROES: return "HEROES";
        case Side::DUNGEON_MASTER: return "DUNGEON_MASTER";
    }
    return "?";
}

std::string effectName(const CardDefinition& card) {
    if (card.heroEffect) {
        switch (*card.heroEffect) {
            case HeroEffect::ATTACK: return "ATTACK";
            case HeroEffect::HEAL: return "HEAL";
            case HeroEffect::BUFF: return "BUFF";
            case HeroEffect::SPELL: return "SPELL";
        }
    }
    if (card.dmEffect) {
        switch (*card.dmEffect) {
            case DMEffect::SUMMON: return "SUMMON";
            case DMEffect::TRAP: return "TRAP";
            case DMEffect::DEFENSIVE: return "DEFENSIVE";
        }
    }
    return "?";
}

std::string heroClassName(HeroClass heroClass) {
    switch (heroClass) {
        case HeroClass::WARRIOR: return "WARRIOR";
        case HeroClass::RANGER: return "RANGER";
        // TODO(SCRUM-5): CLERIC vs HEALER - see the TODO in GameData.cpp.
        // When Types.h renames HEALER to CLERIC, this line will stop compiling:
        // then change it to  case HeroClass::CLERIC: return "CLERIC";
        case HeroClass::HEALER: return "CLERIC (stored as HeroClass::HEALER)";
    }
    return "?";
}

// Text added at the end of a line when an entry still has temporary values.
std::string temporaryMark(bool temporary) {
    return temporary ? "   [TEMPORARY]" : "";
}

// ===================== Printing the summary =====================

void printCards(const GameData& data) {
    std::cout << "Cards (" << data.getCards().size() << "):\n";
    for (const CardDefinition& card : data.getCards()) {
        std::cout << "  " << card.id << "  \"" << card.name << "\"  "
                  << sideName(card.side) << " " << effectName(card)
                  << "  cost " << card.manaCost << "  value " << card.value
                  << temporaryMark(card.temporary) << "\n";
    }
}

void printHeroes(const GameData& data) {
    std::cout << "Heroes (" << data.getHeroes().size() << "):\n";
    for (const HeroDefinition& hero : data.getHeroes()) {
        std::cout << "  " << hero.id << "  \"" << hero.name << "\"  " << heroClassName(hero.heroClass)
                  << "  health " << hero.maxHealth << "  mana " << hero.maxMana << "  deck:";
        for (const DeckEntry& line : hero.startingDeck) {
            std::cout << " " << line.count << "x " << line.cardId;
        }
        std::cout << temporaryMark(hero.temporary) << "\n";
    }
}

void printMonsters(const GameData& data) {
    std::cout << "Monsters (" << data.getMonsters().size() << "):\n";
    for (const MonsterDefinition& monster : data.getMonsters()) {
        std::cout << "  " << monster.id << "  \"" << monster.name << "\"  health " << monster.maxHealth
                  << "  attack " << monster.attackPower << "  boss " << (monster.boss ? "yes" : "no")
                  << temporaryMark(monster.temporary) << "\n";
    }
}

void printLevel(const LevelDefinition& level) {
    std::cout << "Level " << level.number << "  \"" << level.name << "\"  round limit "
              << level.maxRounds << temporaryMark(level.temporary) << "\n";
    std::cout << "  heroes:";
    for (const std::string& heroId : level.defaultHeroes) {
        std::cout << " " << heroId;
    }
    std::cout << "\n  monsters:";
    for (const LevelMonsterEntry& line : level.monsters) {
        std::cout << " " << line.count << "x " << line.monsterId;
    }
    std::cout << "\n";
}

// Collects a short description of every entry that still has "temporary": true.
std::vector<std::string> findTemporaryEntries(const GameData& data, const LevelDefinition& level) {
    std::vector<std::string> entries;
    for (const CardDefinition& card : data.getCards()) {
        if (card.temporary) entries.push_back("cards.json: card \"" + card.id + "\"");
    }
    for (const HeroDefinition& hero : data.getHeroes()) {
        if (hero.temporary) entries.push_back("heroes.json: hero \"" + hero.id + "\"");
    }
    for (const MonsterDefinition& monster : data.getMonsters()) {
        if (monster.temporary) entries.push_back("monsters.json: monster \"" + monster.id + "\"");
    }
    if (level.temporary) {
        entries.push_back("levels/level_" + std::to_string(level.number) + ".json");
    }
    return entries;
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "Dungeon Party - data check (SCRUM-5)\n";

    if (argc > 2) {
        std::cerr << "Usage: validate_data [dataFolder]\n";
        return 1;
    }

    // Use the folder from the command line, or the default one set by CMake.
    const std::string dataFolder = (argc == 2) ? argv[1] : GameData::defaultDataDirectory();
    std::cout << "Data folder: " << dataFolder << "\n\n";

    try {
        // Loading also checks everything. If something is wrong, a GameDataError
        // is thrown and we jump straight to the "catch" part below.
        const GameData data(dataFolder);              // cards.json, heroes.json, monsters.json
        const LevelDefinition level = data.loadLevel(1); // levels/level_1.json

        printCards(data);
        printHeroes(data);
        printMonsters(data);
        printLevel(level);

        const std::vector<std::string> temporary = findTemporaryEntries(data, level);
        if (!temporary.empty()) {
            std::cout << "\nWARNING: " << temporary.size()
                      << " entries still use TEMPORARY values (to be confirmed by Irma):\n";
            for (const std::string& entry : temporary) {
                std::cout << "  - " << entry << "\n";
            }
        }

        std::cout << "\nData check PASSED";
        if (!temporary.empty()) {
            std::cout << " (with " << temporary.size() << " temporary entries)";
        }
        std::cout << ".\n";
        return 0;

    } catch (const GameDataError& error) {
        // A mistake in the data files: the message names the file and the entry.
        std::cerr << "ERROR: " << error.what() << "\n";
        std::cerr << "Data check FAILED. Fix the problem above and run validate_data again.\n";
        return 1;

    } catch (const std::exception& error) {
        // Anything else that went wrong (should not normally happen).
        std::cerr << "UNEXPECTED ERROR: " << error.what() << "\n";
        std::cerr << "Data check FAILED.\n";
        return 1;
    }
}
