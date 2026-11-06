#include "GameData.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

// Each process owns a new temporary folder; production data is never edited.
struct Fixture {
    fs::path folder;
    explicit Fixture(const fs::path& source) {
        auto token = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = fs::temp_directory_path() /
                ("dungeon-card-tests-" + std::to_string(token) + "-" + std::to_string(attempt));
            if (fs::create_directory(candidate)) { folder = candidate; break; }
        }
        check(!folder.empty(), "Cannot create temporary fixture");
        try {
            for (const char* name : {"cards.json", "heroes.json", "monsters.json", "rules.json"})
                fs::copy_file(source / name, folder / name);
        } catch (...) { cleanup(); throw; }
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
    void cleanup() noexcept {
        if (!folder.empty()) { std::error_code error; fs::remove_all(folder, error); }
    }
    ~Fixture() { cleanup(); }
    json read(const char* name) const {
        std::ifstream input(folder / name);
        return json::parse(input);
    }
    void write(const char* name, const json& data) const {
        std::ofstream output(folder / name);
        output << data.dump(2);
        check(bool(output), "Cannot write fixture");
    }
    void rejects(const std::string& file, const std::string& detail) const {
        try { GameData data(folder.string()); }
        catch (const GameDataError& error) {
            std::string message = error.what();
            check(message.find(file) != std::string::npos &&
                  message.find(detail) != std::string::npos,
                  "Unhelpful error: " + message);
            return;
        }
        throw std::runtime_error("Invalid data was accepted: " + detail);
    }
};

void run(const std::string& name, const fs::path& source) {
    Fixture fixture(source);
    const json original = fixture.read("cards.json");
    if (name == "valid_effects") {
        auto root = original;
        const json base = root["cards"][0];
        for (const char* effect : {"ATTACK", "HEAL", "BUFF", "SPELL", "SHIELD"}) {
            auto card = base;
            card["id"] = std::string("hero_") + effect;
            // IDs must use lowercase letters.
            std::string id = card["id"];
            for (char& c : id) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            card["id"] = id;
            card["effect"] = effect;
            if (id == "hero_spell") {
                card["target"] = "ONE_ENEMY";
                card["freezeRounds"] = 1;
            }
            root["cards"].push_back(card);
        }
        for (const char* effect : {"SUMMON", "TRAP", "DEFENSIVE", "ATTACK", "WEAKNESS"}) {
            auto card = base;
            std::string id = std::string("dm_") + effect;
            for (char& c : id) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            card["id"] = id; card["side"] = "DUNGEON_MASTER"; card["effect"] = effect;
            card["manaCost"] = 0; card["value"] = INT_MAX;
            root["cards"].push_back(card);
        }
        fixture.write("cards.json", root);
        const GameData data(fixture.folder.string());
        check(data.getCards().size() == original["cards"].size() + 10, "Cards lost while loading");
        const auto& attack = data.getCard("dm_attack");
        check(attack.side == Side::DUNGEON_MASTER && attack.dmEffect == DMEffect::ATTACK &&
              !attack.heroEffect && attack.manaCost == 0 && attack.value == INT_MAX,
              "DM attack data was not preserved");
        const auto& spell = data.getCard("hero_spell");
        check(spell.heroEffect == HeroEffect::SPELL && spell.target == SpellTarget::ONE_ENEMY &&
              spell.freezeRounds == 1, "Spell fields were not preserved");
    } else if (name == "missing_file") {
        fs::remove(fixture.folder / "cards.json");
        fixture.rejects("cards.json", "cannot open");
    } else if (name == "malformed_json") {
        { std::ofstream output(fixture.folder / "cards.json"); output << "{ broken"; }
        fixture.rejects("cards.json", "not valid JSON");
    } else if (name == "structure") {
        for (const json& root : {json::array(), json::object(), json{{"cards", "wrong"}},
                                json{{"cards", json::array()}}, json{{"cards", json::array({42})}}}) {
            fixture.write("cards.json", root);
            fixture.rejects("cards.json", "");
        }
    } else if (name == "required_fields") {
        for (const char* field : {"id", "name", "side", "effect", "manaCost", "value", "description", "temporary"}) {
            auto root = original;
            root["cards"][0].erase(field);
            fixture.write("cards.json", root);
            fixture.rejects("cards.json", std::string("missing field \"") + field + "\"");
        }
    } else if (name == "field_types") {
        for (const char* field : {"id", "name", "side", "effect", "description", "temporary"}) {
            auto root = original; root["cards"][0][field] = 42;
            fixture.write("cards.json", root); fixture.rejects("cards.json", field);
        }
        for (const char* field : {"name", "description"}) {
            auto root = original; root["cards"][0][field] = "";
            fixture.write("cards.json", root); fixture.rejects("cards.json", "must not be empty");
        }
    } else if (name == "numbers") {
        for (const char* field : {"manaCost", "value"}) {
            for (const json& value : {json(-1), json(1.5), json("3"), json(true), json(nullptr),
                                      json(2147483648ULL), json(18446744073709551615ULL),
                                      json(-9223372036854775807LL)}) {
                auto root = original; root["cards"][0][field] = value;
                fixture.write("cards.json", root); fixture.rejects("cards.json", field);
            }
        }
    } else if (name == "ids") {
        auto root = original; root["cards"].push_back(root["cards"][0]);
        fixture.write("cards.json", root); fixture.rejects("cards.json", "duplicate id");
        for (const char* id : {"", "Strike", "bad id", "bad-id"}) {
            root = original; root["cards"][0]["id"] = id;
            fixture.write("cards.json", root); fixture.rejects("cards.json", "id");
        }
    } else if (name == "enums") {
        auto root = original; root["cards"][0]["side"] = "UNKNOWN";
        fixture.write("cards.json", root); fixture.rejects("cards.json", "unknown side");
        for (const char* side : {"HEROES", "DUNGEON_MASTER"}) {
            root = original; root["cards"][0]["side"] = side;
            root["cards"][0]["effect"] = std::string(side) == "HEROES" ? "SUMMON" : "HEAL";
            fixture.write("cards.json", root); fixture.rejects("cards.json", "unknown effect");
        }
    } else if (name == "deck_references") {
        const auto heroes = fixture.read("heroes.json");
        auto root = heroes;
        root["heroes"][0]["startingDeck"] = json::array({{{"card", "missing_card"}, {"count", 1}}});
        fixture.write("heroes.json", root); fixture.rejects("heroes.json", "unknown card");
        auto cards = original; auto dm = cards["cards"][0];
        dm["id"] = "dm_attack"; dm["side"] = "DUNGEON_MASTER"; dm["effect"] = "ATTACK";
        cards["cards"].push_back(dm); fixture.write("cards.json", cards);
        root["heroes"][0]["startingDeck"][0]["card"] = "dm_attack";
        fixture.write("heroes.json", root); fixture.rejects("heroes.json", "cannot be in a hero deck");
    } else throw std::runtime_error("Unknown case: " + name);
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: card_data_tests <case> <data-folder>");
        run(argv[1], argv[2]);
        std::cout << "PASS: " << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
