#ifndef DUNGEON_PARTY_CARD_H
#define DUNGEON_PARTY_CARD_H
#include <string>
#include "Targetable.h"

class Player;

class Card {
private:
    std::string name;
    int manaCost;

public:
    Card(const std::string& cardname, int manaCost);
    virtual ~Card() = default;
    const std::string& getName() const;
    int getManaCost() const;
    virtual bool canBePlayedBy(const Player& player) const = 0;
    virtual void applyEffect(Targetable& target) const = 0;
};


#endif //DUNGEON_PARTY_CARD_H
