#ifndef DUNGEON_PARTY_TARGETABLE_H
#define DUNGEON_PARTY_TARGETABLE_H

class Targetable{
public:
    virtual ~Targetable() = default;
    virtual void takeDamage(int amount) = 0;
};

#endif