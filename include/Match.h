#ifndef DUNGEON_PARTY_MATCH_H
#define DUNGEON_PARTY_MATCH_H

#include <memory>
#include <vector>
#include "Hero.h"
#include "DungeonMaster.h"
#include "Monster.h"
#include "Trap.h"

class Match {
    std::string matchId;
    int round = 0;
    Phase phase = Phase::ROUND_START;
    std::optional<Side> winner;
    int maxRounds;
    bool started = false;
    std::size_t activeHero = 0;
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::unique_ptr<Monster>> monsters;
    std::vector<std::unique_ptr<Trap>> traps;
    std::vector<Hero*> heroes;
    DungeonMaster* dm = nullptr;
    void beginTurn(Player& player);
    void checkWinner();
    struct MonsterAction {
        Monster* monster;
        const Card* card;
        Hero* target;
    };
    std::vector<MonsterAction> monsterActions;


public:
    Match(std::string id, int maxRounds = 20);
    void addPlayer(std::unique_ptr<Player> player);
    void addMonster(std::unique_ptr<Monster> monster);
    void start();
    CheckResult validate(const Player& player, const Card& card, const Targetable& target) const;
    void apply(const Card& card, Targetable& target);
    void endTurn(Player& player);
    bool isFinished() const;
    int getRound() const;
    Phase getPhase() const;
    const std::string& getMatchId() const;
    std::optional<Side> getWinner() const;
    Player* getActivePlayer() const;
    void queueMonsterAttack(DungeonMaster& player, Monster& monster, const Card& card, Hero& target);};

#endif