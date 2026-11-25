#ifndef DUNGEON_PARTY_MATCH_H
#define DUNGEON_PARTY_MATCH_H

#include <memory>
#include <vector>
#include "Hero.h"
#include "DungeonMaster.h"
#include "DMCard.h"
#include "Monster.h"
#include "Trap.h"
#include "SwarmManager.h"
#include <string>

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
    std::unique_ptr<Monster> pendingBoss;
    Monster* finalBoss = nullptr;
    int bossArrivalRound = 0;
    std::optional<DMCard> bossAttack;
    SwarmManager swarms;   // swarms and the boss that enter once the battlefield is cleared
    void spawnBoss();
    bool releaseNextGroup(); // next swarm, or the boss after the last one; false if nothing is left
    void beginTurn(Player& player);
    void checkWinner();
    struct MonsterAction {
        Monster* monster;
        const Card* card;
        Hero* target;
    };
    std::vector<MonsterAction> monsterActions;
    const std::vector<std::string>& getActionLog() const;


public:
    Match(std::string id, int maxRounds = 20);
    void addPlayer(std::unique_ptr<Player> player);
    void addMonster(std::unique_ptr<Monster> monster);
    // arrivalRound 1..maxRounds: the boss arrives in that round (older levels).
    // arrivalRound 0: the boss arrives after the heroes defeated every swarm.
    void configureBoss(std::unique_ptr<Monster> boss, int arrivalRound, int attackPower);
    // Swarms enter one after another: the next one when all monsters on the field are defeated.
    void addSwarm(std::string name, std::vector<std::unique_ptr<Monster>> monsters);
    int getSwarmNumber() const { return swarms.swarmsReleased(); } // 1 = first swarm is fighting
    int getSwarmCount() const { return swarms.swarmCount(); }
    bool hasBossArrived() const { return finalBoss != nullptr; }
    void start();
    CheckResult validate(const Player& player, const Card& card, const Targetable& target) const;
    void apply(const Card& card, Targetable& target);
    void endTurn(Player& player);
    bool isFinished() const;
    int getRound() const;
    Phase getPhase() const;
    const std::string& getMatchId() const;
    std::optional<Side> getWinner() const;
    const std::vector<Hero*>& getHeroes() const { return heroes; }
    const std::vector<std::unique_ptr<Monster>>& getMonsters() const { return monsters; }
    Player* getActivePlayer() const;
    bool canQueueBossAttack() const;
    void queueBossAttack(DungeonMaster& player, Hero& target);
    void queueMonsterAttack(DungeonMaster& player, Monster& monster, const Card& card, Hero& target);
    std::vector<std::string> actionLog;
    void addPublicLog(const std::string& message);
};

#endif