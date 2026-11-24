#include "Hero.h"
#include "HeroCard.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

Hero::Hero(std::string id, std::string name, HeroType heroType, int maxMana, bool bot)
    : Player(std::move(id), std::move(name), maxMana, bot), health(heroType.maxHealth), maxHealth(heroType.maxHealth), type(heroType) {

    if (maxHealth <= 0) {
        throw std::invalid_argument("Hero needs positive HP");
    }
}
    
bool Hero::acceptsCard(const Card& c) const{
    return dynamic_cast<const HeroCard*>(&c) && c.canBePlayedBy(*this);
}

int Hero::getHealth() const{
    return health;
}

void Hero::takeDamage(int amount) {
    if (amount <= 0) {
        return;
    }

    if (shield >= amount) {
        shield -= amount;
        return;
    }

    amount -= shield;
    shield = 0;

    health = std::max(0, health - amount);
}

void Hero::heal(int amount) {
    if (amount <= 0) {
        return;
    }

    int missingHealth = maxHealth - health;

    if (amount > missingHealth) {
        health = maxHealth;
    } else {
        health += amount;
    }
}

bool Hero::isAlive() const{
    return health > 0;
}

const HeroType& Hero::getType() const{
    return type;
}

void Hero::applyWeakness(int amount) {
    if (amount <= 0) {
        return;
    }

    // Weakness does not stack. Keep the strongest reduction.
    if (amount > attackReduction) {
        attackReduction = amount;
    }
}

int Hero::consumeAttackReduction() {
    int reduction = attackReduction;
    attackReduction = 0;

    return reduction;
}

int Hero::getAttackReduction() const {
    return attackReduction;
}

void Hero::addShield(int amount) {
    if (amount <= 0) {
        return;
    }

    shield += amount;
}

int Hero::getShield() const {
    return shield;
}

void Hero::armManaTrap(TrapEffect effect) {
    if (!isAlive()) {
        return;
    }

    manaTrapActive = true;
    manaSpentForTrap = 0;
    activeTrapEffect = effect;
}

std::optional<TrapEffect> Hero::recordManaSpentForTrap(int amount) {
    if (!manaTrapActive || amount <= 0) {
        return std::nullopt;
    }

    // Exactly 20 does not trigger the trap.
    if (amount > 20 - manaSpentForTrap) {
        TrapEffect triggeredEffect = activeTrapEffect;

        clearManaTrap();

        return triggeredEffect;
    }

    manaSpentForTrap += amount;

    return std::nullopt;
}

void Hero::clearManaTrap() {
    manaTrapActive = false;
    manaSpentForTrap = 0;
}

int Hero::calculateShield(int baseAmount, bool targetingSelf) const {
    return baseAmount;
}

bool Hero::canHealAllies() const {
    return false;
}

bool Hero::canShieldAllies() const {
    return false;
}

int Hero::getMaxHealth() const {
    return maxHealth;
}
