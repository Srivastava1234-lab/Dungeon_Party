#include "Warrior.h"
#include <utility>

Warrior::Warrior(std::string id, std::string name, int maxHealth, int maxMana, bool bot)
    : Hero(std::move(id), std::move(name), HeroType{HeroClass::WARRIOR, maxHealth}, maxMana, bot) {
}

int Warrior::calculateShield(int baseAmount, bool targetingSelf) const {
    if (targetingSelf) {
        return baseAmount + 3;
    }

    return baseAmount;
}

std::string Warrior::getClassName() const {
    return "Warrior";
}