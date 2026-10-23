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
//   0 = the data is OK (temporary values and missing pictures are only warnings)
//   1 = the data has an error (the message says which file and which entry)
//
// This program only uses GameData.h / GameData.cpp. It does not need the game window,
// OpenGL or any of the game logic classes, so it builds quickly.

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
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
        if (hero.startingDeck.empty()) {
            std::cout << " (empty)";
        }
        for (const DeckEntry& line : hero.startingDeck) {
            std::cout << " " << line.count << "x " << line.cardId;
        }
        std::cout << temporaryMark(hero.temporary) << "\n";
        std::cout << "      image: " << hero.image << "\n";
    }
}

void printMonsters(const GameData& data) {
    std::cout << "Monsters (" << data.getMonsters().size() << "):\n";
    for (const MonsterDefinition& monster : data.getMonsters()) {
        std::cout << "  " << monster.id << "  \"" << monster.name << "\"  health " << monster.maxHealth
                  << "  attack " << monster.attackPower << "  boss " << (monster.boss ? "yes" : "no")
                  << temporaryMark(monster.temporary) << "\n";
        std::cout << "      image: " << monster.image << "\n";
    }
}

void printRules(const RulesDefinition& rules) {
    std::cout << "Rules (rules.json):" << temporaryMark(rules.temporary) << "\n"
              << "  max cards played per turn: " << rules.maxCardsPlayedPerTurn << "\n"
              << "  cards drawn per turn:      " << rules.cardsDrawnPerTurn << "\n"
              << "  shuffle deck at start:     " << (rules.shuffleDeckAtMatchStart ? "yes" : "no") << "\n"
              << "  refill mana each round:    " << (rules.refillManaEachRound ? "yes" : "no") << "\n"
              << "  max mana increase / round: " << rules.maxManaIncreasePerRound << "\n"
              << "  hero turn time limit:      " << rules.heroTurnTimeLimitSeconds << " s\n";
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
        std::cout << " " << line.countPerHero << "x " << line.monsterId << " per hero";
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
    if (data.getRules().temporary) {
        entries.push_back("rules.json");
    }
    if (level.temporary) {
        entries.push_back("levels/level_" + std::to_string(level.number) + ".json");
    }
    return entries;
}

// Picture paths are relative to the project folder, which is the folder that contains data/.
// Returns true if the picture file exists. (Never throws: errors count as "not found".)
bool imageExists(const std::string& dataFolder, const std::string& image) {
    std::filesystem::path folder = std::filesystem::absolute(dataFolder).lexically_normal();
    if (folder.filename().empty()) {
        folder = folder.parent_path(); // the path ended with '/', e.g. "data/"
    }
    std::error_code error;
    return std::filesystem::exists(folder.parent_path() / image, error);
}

// Collects every hero/monster picture that cannot be found on disk.
std::vector<std::string> findMissingImages(const GameData& data, const std::string& dataFolder) {
    std::vector<std::string> missing;
    for (const HeroDefinition& hero : data.getHeroes()) {
        if (!imageExists(dataFolder, hero.image)) {
            missing.push_back(hero.image + "  (hero \"" + hero.id + "\")");
        }
    }
    for (const MonsterDefinition& monster : data.getMonsters()) {
        if (!imageExists(dataFolder, monster.image)) {
            missing.push_back(monster.image + "  (monster \"" + monster.id + "\")");
        }
    }
    return missing;
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
        const GameData data(dataFolder);                 // cards, heroes, monsters, rules
        const LevelDefinition level = data.loadLevel(1); // levels/level_1.json

        printCards(data);
        printHeroes(data);
        printMonsters(data);
        printRules(data.getRules());
        printLevel(level);

        const std::vector<std::string> temporary = findTemporaryEntries(data, level);
        if (!temporary.empty()) {
            std::cout << "\nWARNING: " << temporary.size()
                      << " entries still use TEMPORARY values (not confirmed as final yet):\n";
            for (const std::string& entry : temporary) {
                std::cout << "  - " << entry << "\n";
            }
        }

        // A missing picture is only a warning: the data itself is still valid.
        const std::vector<std::string> missingImages = findMissingImages(data, dataFolder);
        if (!missingImages.empty()) {
            std::cout << "\nWARNING: " << missingImages.size()
                      << " picture file(s) not found (paths are relative to the project folder):\n";
            for (const std::string& image : missingImages) {
                std::cout << "  - " << image << "\n";
            }
        }

        std::cout << "\nData check PASSED";
        if (!temporary.empty() || !missingImages.empty()) {
            std::cout << " (with " << temporary.size() << " temporary entries, "
                      << missingImages.size() << " missing pictures)";
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
