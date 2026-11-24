#ifndef DUNGEON_PARTY_WIZARD_H
#define DUNGEON_PARTY_WIZARD_H

#include "Hero.h"

class Wizard : public Hero {
public:
    Wizard(std::string id, std::string name, int maxHealth, int maxMana, bool bot = false);
    std::string getClassName() const override;
};

#endif