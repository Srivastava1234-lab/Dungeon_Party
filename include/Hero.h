#ifndef DUNGEON_PARTY_HERO_H
#define DUNGEON_PARTY_HERO_H
#include "Player.h"
#include "Targetable.h"
#include "Types.h"
#include <optional>
#include "TimedEffects.h"

class Hero : public Player, public Targetable{
private:
    int health;
    HeroType type;
    int attackReduction = 0;
    int shield = 0;
    TimedEffects timedEffects;
    bool manaTrapActive = false;
    int manaSpentForTrap = 0;
    TrapEffect activeTrapEffect = TrapEffect::DAMAGE;

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
    void addTimedEffect(TimedEffectType type, int value, int turns);
    const TimedEffects& getTimedEffects() const { return timedEffects; }
    void tickEffects() { timedEffects.tick(); }
};


#endif //DUNGEON_PARTY_HERO_H
