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
public:
    HeroCard(std::string name, int cost, HeroEffect effect, int value, std::optional<HeroClass> requiredClass = std::nullopt, SupportTarget supportTarget = SupportTarget::SELF_ONLY);
    HeroEffect getEffect() const;
    int getValue() const;
    bool canBePlayedBy(const Player& player) const override;
    void applyEffect(Targetable& target) const override;
    SupportTarget getSupportTarget() const;
};

#endif