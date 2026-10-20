#include "../include/Hero.h"
#include <algorithm>

Hero::Hero(int startingHealth)
    : health(std::max(0, startingHealth)){

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

bool Hero::isAlive() const{
    return health > 0;
}