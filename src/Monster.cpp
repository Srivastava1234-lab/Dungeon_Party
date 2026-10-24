#include "Monster.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
Monster::Monster(std::string n, int h, bool b, int l)
    : name(std::move(n)),
    health(h),
    maxHealth(h),
    boss(b), 
    level(l) {
    if (h <= 0 || l <= 0){
        throw std::invalid_argument("Invalid monster values");
    }      
}

void Monster::takeDamage(int n) { 
    if (n > 0){
     health -= std::min(health, n); 
    }
}

void Monster::attack(const Card& c, Targetable& t) const {
    if (!isAlive()){
        throw std::logic_error("Defeated monsters cannot attack");
    }

    c.applyEffect(t);
}

const std::string& Monster::getName() const {
    return name;
}

int Monster::getHealth() const {
    return health;
}

int Monster::getMaxHealth() const {
    return maxHealth;
}

bool Monster::isAlive() const {
    return health > 0;
}

bool Monster::isBoss() const {
    return boss;
}

int Monster::getLevel() const {
    return level;
}


