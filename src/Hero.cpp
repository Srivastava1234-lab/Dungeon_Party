#include "Hero.h"
#include "HeroCard.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

Hero::Hero(std::string id, std::string n, HeroType t, int m, bool b)
    : Player(std::move(id), 
    std::move(n), m, b), 
    health(t.maxHealth),
    type(t){
        if(t.maxHealth <= 0){
            throw std::invalid_argument("Hero needs positive hp");
        }
    } 
    
bool Hero::acceptsCard(const Card& c) const{
    return dynamic_cast<const HeroCard*>(&c) && c.canBePlayedBy(*this);
}

int Hero::getHealth() const{
    return health;
}

void Hero::takeDamage(int amount){
    if(amount <= 0){
        return;
    }

    health = std::max(0, health-amount);
}

void Hero::heal(int amount) {
    if (amount <= 0) {
        return;
    }

    int missingHealth = type.maxHealth - health;

    if (amount > missingHealth) {
        health = type.maxHealth;
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