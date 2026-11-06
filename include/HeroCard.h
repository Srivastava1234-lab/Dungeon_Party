#ifndef DUNGEON_PARTY_HEROCARD_H
#define DUNGEON_PARTY_HEROCARD_H
#include "Card.h"
#include "Types.h"

class HeroCard : public Card {
    HeroEffect effect;
    int value;
    SpellTarget target;
    int freezeRounds;
public:
    HeroCard(std::string name, int cost, HeroEffect effect, int value,
             SpellTarget target = SpellTarget::ONE_ENEMY, int freezeRounds = 0);
    SpellTarget getTarget() const { return target; }
    int getFreezeRounds() const { return freezeRounds; }
    HeroEffect getEffect() const;
    int getValue() const;
    bool canBePlayedBy(const Player& player) const override;
    void applyEffect(Targetable& target) const override;
};

#endif