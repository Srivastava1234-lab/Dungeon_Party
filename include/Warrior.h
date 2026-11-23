#ifndef DUNGEON_PARTY_WARRIOR_H
#define DUNGEON_PARTY_WARRIOR_H

#include "Hero.h"

class Warrior : public Hero {
public:
    Warrior(std::string id, std::string name, int maxHealth, int maxMana, bool bot = false);
    int calculateShield(int baseAmount, bool targetingSelf) const override;
    std::string getClassName() const override;
};

#endif