#include "Wizard.h"
#include <utility>

Wizard::Wizard(std::string id, std::string name, int maxHealth, int maxMana, bool bot)
    : Hero(std::move(id), std::move(name), HeroType{HeroClass::WIZARD, maxHealth}, maxMana, bot) {
}

std::string Wizard::getClassName() const {
    return "Wizard";
}