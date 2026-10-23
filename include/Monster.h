#ifndef DUNGEON_PARTY_MONSTER_H
#define DUNGEON_PARTY_MONSTER_H
#include "Card.h"

class Monster : public Targetable {
    std::string name;
    int health;
    bool boss;
    int level;
public:
    Monster(std::string name, int health, bool boss = false, int level = 1);
    const std::string& getName() const;
    int getHealth() const;
    bool isAlive() const;
    bool isBoss() const;
    int getLevel() const;
    void takeDamage(int amount) override;
    void attack(const Card& card, Targetable& target) const;
};


#endif