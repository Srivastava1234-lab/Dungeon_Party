#include "../include/Card.h"
#include <algorithm>

Card::Card(const std::string& cardName, int cost, int damage)
    : name(cardName),
    manaCost(std::max(0, cost)),
    damage(std::max(0, damage)){

    }

const std::string& Card::getName() const{
    return name;
}

int Card::getManaCost() const{
    return manaCost;
}

int Card::getDamage() const{
    return damage;
}
