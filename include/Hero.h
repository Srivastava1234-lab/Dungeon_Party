#ifndef DUNGEON_PARTY_HERO_H
#define DUNGEON_PARTY_HERO_H
#include "Player.h"
#include "Targetable.h"
#include "Types.h"
#include "TimedEffects.h"
#include <optional>

class Hero : public Player, public Targetable{
private:
    int health;
    HeroType type;
    int attackReduction = 0;
    int shield = 0;
    bool manaTrapActive = false;
    int manaSpentForTrap = 0;
    TrapEffect activeTrapEffect = TrapEffect::DAMAGE;
    TimedEffects timedEffects;

public:
    Hero(std::string id, std::string name, HeroType type, int maxMana, bool bot = false);
    bool acceptsCard(const Card& card) const override;
    int getHealth() const; 
    const HeroType& getType() const;
    void takeDamage(int amount) override;
    void heal(int amount);
    bool isAlive() const;
    void applyWeakness(int amount);
    int consumeAttackReduction();
    int getAttackReduction() const;
    void addShield(int amount);
    int getShield() const;
    void armManaTrap(TrapEffect effect);
    std::optional<TrapEffect> recordManaSpentForTrap(int amount);
    void clearManaTrap();
    bool hasManaTrap() const;          // SCRUM-54: read-only, for the DM's state
    TrapEffect getTrapEffect() const;  // only meaningful while hasManaTrap()
    void addTimedEffect(TimedEffectType type, int value, int rounds);
    const TimedEffects& getTimedEffects() const;
    void tickEffects();
};


#endif //DUNGEON_PARTY_HERO_H
