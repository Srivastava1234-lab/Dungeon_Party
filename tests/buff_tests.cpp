#include "Match.h"
#include "HeroCard.h"
#include "GameData.h"
#include "Warrior.h"
#include "Healer.h"
#include "Wizard.h"
#include <iostream>
#include <memory>
#include <stdexcept>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int turns(const Hero& hero) {
    return hero.getTimedEffects().roundsLeft(TimedEffectType::BUFF);
}
struct Fixture {
    Match match{"buff-test", 20};
    Hero* caster;
    Hero* ally;
    DungeonMaster* dm;
    Monster* monster;
    explicit Fixture(HeroClass heroClass = HeroClass::WARRIOR) {
        std::unique_ptr<Hero> first;
        switch (heroClass) {
            case HeroClass::WARRIOR: first = std::make_unique<Warrior>("first", "First", 100, 100); break;
            case HeroClass::HEALER: first = std::make_unique<Healer>("first", "First", 100, 100); break;
            case HeroClass::WIZARD: first = std::make_unique<Wizard>("first", "First", 100, 100); break;
            default: throw std::invalid_argument("No hero subclass for this test");
        }
        auto second = std::make_unique<Healer>("second", "Second", 100, 100);
        caster = first.get(); ally = second.get();
        auto master = std::make_unique<DungeonMaster>("dm", "DM", 100, false);
        dm = master.get();
        auto enemy = std::make_unique<Monster>("Enemy", 1000);
        monster = enemy.get();
        for (int i = 0; i < 8; ++i) {
            first->addToDeck(std::make_unique<HeroCard>("Battle Cry", 8, HeroEffect::BUFF, 5));
            second->addToDeck(std::make_unique<HeroCard>("Battle Cry", 8, HeroEffect::BUFF, 5));
        }
        match.addPlayer(std::move(first)); match.addPlayer(std::move(second));
        match.addPlayer(std::move(master)); match.addMonster(std::move(enemy)); match.start();
    }
    void nextRound() {
        match.endTurn(*caster); match.endTurn(*ally); match.endTurn(*dm);
    }
};
int main() {
    try {
        GameData data(GameData::defaultDataDirectory());
        const auto* definition = &data.getCard("battle_cry");
        check(definition && definition->value == 5 && definition->manaCost == 8 &&
              definition->durationTurns == 2 && definition->temporary, "Battle Cry data");
        for (auto heroClass : {HeroClass::WARRIOR, HeroClass::WIZARD, HeroClass::HEALER}) {
            Fixture f(heroClass);
            f.match.apply(f.caster->getHandCard(0), *f.ally);
            check(turns(*f.ally) == 2 && f.caster->getMana() == 92, "all classes can buff allies");
            f.match.endTurn(*f.caster);
            check(turns(*f.ally) == 2, "caster turn must not consume ally duration");
            f.match.endTurn(*f.ally);
            check(turns(*f.ally) == 1, "target first turn");
            f.match.endTurn(*f.dm);
            check(turns(*f.ally) == 1, "round boundary must not tick");
        }
        // Shield bonuses stay in the hero subclasses and never consume Buff or Weakness.
        for (auto heroClass : {HeroClass::WARRIOR, HeroClass::HEALER, HeroClass::WIZARD}) {
            Fixture f(heroClass);
            f.match.apply(f.caster->getHandCard(0), *f.caster);
            f.caster->applyWeakness(2);
            f.caster->addToDeck(std::make_unique<HeroCard>("Shield", 1, HeroEffect::SHIELD, 5));
            while (f.caster->getHandSize() < 8) f.caster->drawCard();
            Hero* target = heroClass == HeroClass::HEALER ? f.ally : f.caster;
            f.match.apply(f.caster->getHandCard(7), *target);
            check(target->getShield() == (heroClass == HeroClass::WIZARD ? 5 : 8),
                  "polymorphic shield bonus works while buffed");
            check(turns(*f.caster) == 2 && f.caster->getAttackReduction() == 2,
                  "shield leaves Buff and Weakness active");
        }
        // Refreshing a buff still spends mana and can trigger every trap variant.
        for (auto trap : {TrapEffect::DAMAGE, TrapEffect::DISCARD, TrapEffect::MANA_DRAIN}) {
            Fixture f;
            f.caster->armManaTrap(trap);
            for (int i = 0; i < 3; ++i) f.match.apply(f.caster->getHandCard(0), *f.caster);
            check(!f.caster->hasManaTrap(), "buff mana spending triggers the armed trap");
            check(turns(*f.caster) == 2 && f.caster->getTimedEffects().valueOf(TimedEffectType::BUFF) == 5,
                  "trap interaction preserves one refreshed buff");
            check(f.caster->getHealth() == (trap == TrapEffect::DAMAGE ? 90 : 100),
                  "damage trap activates after repeated buff plays");
            check(f.caster->getMana() == (trap == TrapEffect::MANA_DRAIN ? 71 : 76),
                  "all buff plays spend mana and mana-drain trap applies");
            check(f.caster->getHandSize() == (trap == TrapEffect::DISCARD ? 3 : 4),
                  "discard trap removes one additional card");
        }
        {
            Fixture f;
            f.match.apply(f.caster->getHandCard(0), *f.caster);
            f.nextRound();
            check(turns(*f.caster) == 1, "self cast current turn counts");
            f.match.apply(f.caster->getHandCard(0), *f.caster);
            check(turns(*f.caster) == 2 &&
                  f.caster->getTimedEffects().valueOf(TimedEffectType::BUFF) == 5 &&
                  f.caster->getMana() == 92, "refresh pays mana and does not stack");
            f.nextRound(); f.nextRound();
            check(turns(*f.caster) == 0, "expires after two target turns");
        }
        for (auto effect : {HeroEffect::ATTACK, HeroEffect::SPELL, HeroEffect::FIREBALL}) {
            Fixture f;
            f.match.apply(f.caster->getHandCard(0), *f.caster);
            f.caster->applyWeakness(2);
            f.caster->addToDeck(std::make_unique<HeroCard>("Damage", 1, effect, 10));
            // Drain the remaining buff-only deck so the damage card is drawn deterministically.
            while (f.caster->getHandSize() < 8) f.caster->drawCard();
            const Card& attack = f.caster->getHandCard(7);
            f.match.apply(attack, *f.monster);
            check(f.monster->getHealth() == 987, "buff and weakness combine on damage");
            check(turns(*f.caster) == 2 && f.caster->getAttackReduction() == 0,
                  "damage retains buff and consumes weakness");
        }
        {
            Fixture f;
            Warrior outsider("other", "Other", 100, 100);
            const auto hand = f.caster->getHandSize();
            check(!f.match.validate(*f.caster, f.caster->getHandCard(0), outsider).valid,
                  "foreign hero rejected");
            check(!f.match.validate(*f.caster, f.caster->getHandCard(0), *f.monster).valid,
                  "monster rejected");
            check(f.caster->getMana() == 100 && f.caster->getHandSize() == hand,
                  "invalid validation preserves resources");
            f.match.apply(f.caster->getHandCard(0), *f.ally);
            f.ally->takeDamage(100);
            check(turns(*f.ally) == 0, "death clears buff");
            check(!f.match.validate(*f.caster, f.caster->getHandCard(0), *f.ally).valid,
                  "dead target rejected");
        }
        std::cout << "Battle Cry regression checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
