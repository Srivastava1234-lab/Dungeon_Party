#ifndef DUNGEON_PARTY_TRAP_H
#define DUNGEON_PARTY_TRAP_H
#include "Hero.h"

class Trap {
    int damage;
    bool revealed = false;
public:
    explicit Trap(int damage);
    void trigger(Hero& hero);
    bool isRevealed() const;
};


#endif