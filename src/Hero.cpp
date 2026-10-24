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

    health -= std::min(health, amount); // never drops below 0
}

void Hero::heal(int amount){
    if(amount <= 0){
        return;
    }

    health = std::min(type.maxHealth, health + std::min(amount, type.maxHealth)); // capped at maxHealth, no int overflow
}

bool Hero::isAlive() const{
    return health > 0;
}

const HeroType& Hero::getType() const{
    return type;
}