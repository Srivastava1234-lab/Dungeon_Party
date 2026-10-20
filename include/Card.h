#ifndef DUNGEON_PARTY_CARD_H
#define DUNGEON_PARTY_CARD_H
#include <string>


class Card {
private:
    std::string name;
    int manaCost;
    int damage;

public:
    Card(const std::string& cardname, int cost, int attackDamage);
    const std::string& getName() const;
    int getManaCost() const;
    int getDamage() const;
    
};


#endif //DUNGEON_PARTY_CARD_H
