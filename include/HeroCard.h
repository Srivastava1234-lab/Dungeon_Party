#ifndef DUNGEON_PARTY_HEROCARD_H
#define DUNGEON_PARTY_HEROCARD_H
#include "Card.h"
#include "Types.h"
#include <optional>

class HeroCard : public Card {
    HeroEffect effect;
    int value;
    std::optional<HeroClass> requiredClass;
    SupportTarget supportTarget;
    SpellTarget target = SpellTarget::ONE_ENEMY; // spells (Hülya, Sprint 2)
    int freezeRounds = 0;
public:
    // Class-restricted / support cards (Kagan)
    HeroCard(std::string name, int cost, HeroEffect effect, int value, std::optional<HeroClass> requiredClass = std::nullopt, SupportTarget supportTarget = SupportTarget::SELF_ONLY);
    // Spells with a target rule and optional freeze (Hülya)
    HeroCard(std::string name, int cost, HeroEffect effect, int value, SpellTarget target, int freezeRounds = 0);
    SpellTarget getTarget() const { return target; }
    int getFreezeRounds() const { return freezeRounds; }
    HeroEffect getEffect() const;
    int getValue() const;
    bool canBePlayedBy(const Player& player) const override;
    void applyEffect(Targetable& target) const override;
    SupportTarget getSupportTarget() const;
};

#endif