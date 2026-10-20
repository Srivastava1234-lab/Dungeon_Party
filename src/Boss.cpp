#include "../include/Boss.h"
#include <algorithm>

Boss::Boss(int startingHealth)
    : health(std::max(0, startingHealth)){
    }

int Boss::getHealth() const{
    return health;
}


void Boss::takeDamage(int amount){
    if(amount <= 0){
        return;
    }

    health = std::max(0, health-amount);
}

bool Boss::isAlive() const{
    return health > 0;
}
