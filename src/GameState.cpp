#include "GameState.h"
#include <nlohmann/json.hpp>
#include "DungeonMaster.h"
#include "Hero.h"
#include "HeroCard.h"
#include "Match.h"
#include "Monster.h"

GameState makeGameState(const Match& match, const DungeonMaster& dm, const Player* viewer)
{
    GameState state;
    state.round = match.getRound();
    state.phase = match.getPhase();
    state.winner = match.getWinner();

    const Player* active = match.getActivePlayer();
    if (active) state.activePlayerId = active->getPlayerId();

    for (const Hero* h : match.getHeroes())
    {
        state.heroes.push_back(HeroState{
            h->getPlayerId(),
            h->getName(),
            h->getType().heroClass,
            h->getHealth(),
            h->getType().maxHealth,
            h->getMana(),
            h->getMaxMana(),
            h->getShield(),
            static_cast<int>(h->getHandSize()),
            h->isAlive()
        });
    }

    // Dead monsters stay in the list (alive = false), the screen skips them.
    for (const auto& m : match.getMonsters())
    {
        state.monsters.push_back(MonsterState{
            m->getName(),
            m->getHealth(),
            m->getMaxHealth(),
            m->isBoss(),
            m->isAlive(),
            m->getBurnTurnsRemaining()
        });
    }

    state.dmMana = dm.getMana();
    state.dmMaxMana = dm.getMaxMana();
    state.dmHandSize = static_cast<int>(dm.getHandSize());

    if (viewer)
    {
        for (std::size_t i = 0; i < viewer->getHandSize(); ++i)
        {
            const Card& card = viewer->getHandCard(i);
            const HeroCard* hc = dynamic_cast<const HeroCard*>(&card);
            state.myHand.push_back(CardState{
                card.getName(),
                card.getManaCost(),
                hc ? hc->getEffect() : HeroEffect::ATTACK,
                hc ? hc->getValue() : 0
            });
        }
    }
    return state;
}

// These macros write to_json / from_json for the simple structs (enums are stored as numbers).
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HeroState, id, name, heroClass, health, maxHealth, mana, maxMana, shield, handSize, alive)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MonsterState, name, health, maxHealth, boss, alive, burnTurns)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CardState, name, manaCost, effect, value)

std::string gameStateToJson(const GameState& state)
{
    nlohmann::json j;
    j["round"] = state.round;
    j["phase"] = state.phase;
    j["activePlayerId"] = state.activePlayerId;
    j["winner"] = state.winner ? static_cast<int>(*state.winner) : -1;   // -1 = nobody yet
    j["heroes"] = state.heroes;
    j["monsters"] = state.monsters;
    j["dmMana"] = state.dmMana;
    j["dmMaxMana"] = state.dmMaxMana;
    j["dmHandSize"] = state.dmHandSize;
    j["myHand"] = state.myHand;
    return j.dump();
}

GameState gameStateFromJson(const std::string& text)
{
    nlohmann::json j = nlohmann::json::parse(text);
    GameState state;
    state.round = j.at("round").get<int>();
    state.phase = j.at("phase").get<Phase>();
    state.activePlayerId = j.at("activePlayerId").get<std::string>();
    int w = j.at("winner").get<int>();
    if (w >= 0) state.winner = static_cast<Side>(w);
    state.heroes = j.at("heroes").get<std::vector<HeroState>>();
    state.monsters = j.at("monsters").get<std::vector<MonsterState>>();
    state.dmMana = j.at("dmMana").get<int>();
    state.dmMaxMana = j.at("dmMaxMana").get<int>();
    state.dmHandSize = j.at("dmHandSize").get<int>();
    state.myHand = j.at("myHand").get<std::vector<CardState>>();
    return state;
}