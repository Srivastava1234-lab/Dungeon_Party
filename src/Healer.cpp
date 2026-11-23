#include "Healer.h"
#include <utility>

Healer::Healer(std::string id, std::string name, int maxHealth, int maxMana, bool bot)
    : Hero(std::move(id), std::move(name), HeroType{HeroClass::HEALER, maxHealth}, maxMana, bot) {
}

int Healer::calculateShield(int baseAmount, bool targetingSelf) const {
    if (!targetingSelf) {
        return baseAmount + 3;
    }

    return baseAmount;
}

bool Healer::canHealAllies() const {
    return true;
}

bool Healer::canShieldAllies() const {
    return true;
}

std::string Healer::getClassName() const {
    return "Healer";
}