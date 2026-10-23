#ifndef DUNGEON_PARTY_DMCARD_H
#define DUNGEON_PARTY_DMCARD_H
#include "Card.h"
#include "Types.h"

class DMCard : public Card {
private:
    DMEffect effect;
    int value;
public:
    DMCard(std::string name, int cost, DMEffect effect, int value);
    DMEffect getEffect() const; // dont forget to implement
    int getValue() const; // dont forget to implement
    bool canBePlayedBy(const Player& player) const override;
    void applyEffect(Targetable& target) const override;
};

#endif