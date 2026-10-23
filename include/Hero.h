#ifndef DUNGEON_PARTY_HERO_H
#define DUNGEON_PARTY_HERO_H
#include "Player.h"
#include "Targetable.h"
#include "Types.h"

class Hero : public Player, public Targetable{
private:
    int health;
    HeroType type;

public:
    Hero(std::string id, std::string name, HeroType type, int maxMana, bool bot = false);
    bool acceptsCard(const Card& card) const override;
    int getHealth() const; 
    const HeroType& getType() const;
    void takeDamage(int amount) override;
    void heal(int amount);
    bool isAlive() const;
};


#endif //DUNGEON_PARTY_HERO_H
