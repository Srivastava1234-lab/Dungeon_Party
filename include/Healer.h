#ifndef DUNGEON_PARTY_HEALER_H
#define DUNGEON_PARTY_HEALER_H

#include "Hero.h"

class Healer : public Hero {
public:
    Healer(std::string id, std::string name, int maxHealth, int maxMana, bool bot = false);
    int calculateShield(int baseAmount, bool targetingSelf) const override;
    bool canHealAllies() const override;
    bool canShieldAllies() const override;
    std::string getClassName() const override;
};

#endif