#include "SwarmManager.h"

#include <stdexcept>
#include <utility>

void SwarmManager::addSwarm(std::string name, std::vector<std::unique_ptr<Monster>> monsters) {
    if (monsters.empty()) throw std::invalid_argument("A swarm needs at least one monster");
    for (const auto& monster : monsters)
        if (!monster || monster->isBoss()) throw std::invalid_argument("Swarms contain normal monsters only");
    swarms.push_back(Swarm{std::move(name), std::move(monsters)});
}

void SwarmManager::setBoss(std::unique_ptr<Monster> newBoss) {
    if (!newBoss || !newBoss->isBoss()) throw std::invalid_argument("The final boss must be a boss monster");
    boss = std::move(newBoss);
}

SwarmManager::Swarm SwarmManager::takeNextSwarm() {
    if (!hasNextSwarm()) throw std::logic_error("No swarm left");
    return std::move(swarms[nextSwarm++]);
}

std::unique_ptr<Monster> SwarmManager::takeBoss() {
    return std::move(boss);
}
