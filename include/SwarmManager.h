#ifndef DUNGEON_PARTY_SWARM_MANAGER_H
#define DUNGEON_PARTY_SWARM_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "Monster.h"

// SwarmManager
// ------------
// Keeps the monsters of a level that have not entered the battle yet:
// the swarms (waves) in order, and the Dungeon Master's final boss.
//
//   swarm 1 -> (all defeated) -> swarm 2 -> (all defeated) -> ... -> final boss
//
// Match asks it for the next group whenever the battlefield is cleared. Taking this out of
// Match keeps Match smaller (architecture notebook: "split Match into smaller classes").
class SwarmManager {
public:
    struct Swarm {
        std::string name;                               // shown in the log, e.g. "Goblin raiders"
        std::vector<std::unique_ptr<Monster>> monsters;
    };

    void addSwarm(std::string name, std::vector<std::unique_ptr<Monster>> monsters);
    void setBoss(std::unique_ptr<Monster> boss);        // arrives after the last swarm

    bool hasNextSwarm() const { return nextSwarm < swarms.size(); }
    bool hasPendingBoss() const { return boss != nullptr; }
    bool isEmpty() const { return !hasNextSwarm() && !hasPendingBoss(); }

    Swarm takeNextSwarm();                              // the next swarm, in order
    std::unique_ptr<Monster> takeBoss();

    int swarmCount() const { return static_cast<int>(swarms.size()); }
    int swarmsReleased() const { return static_cast<int>(nextSwarm); } // 1 = the first swarm is fighting

private:
    std::vector<Swarm> swarms;
    std::size_t nextSwarm = 0;
    std::unique_ptr<Monster> boss;
};

#endif
