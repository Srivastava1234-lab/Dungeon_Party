#include "Card.h"
#include "HeroCard.h"
#include "DMCard.h"
#include "Hero.h"
#include "DungeonMaster.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "Monster.h"

Card::Card(const std::string& n, int cost)
    : name(n),
    manaCost(std::max(0, cost)){
        if (cost < 0){
             throw std::invalid_argument("negative mana cost");
        } 
    }

HeroCard::HeroCard(std::string n, int c, HeroEffect e, int v, std::optional<HeroClass> requiredClass, SupportTarget supportTarget) 
    : Card(std::move(n), c), effect(e), value(v), requiredClass(requiredClass), supportTarget(supportTarget){
        if (v < 0){
            throw std::invalid_argument("negative effect value");
        }
    }

HeroCard::HeroCard(std::string n, int c, HeroEffect e, int v, SpellTarget spellTarget, int freeze)
    : HeroCard(std::move(n), c, e, v) {
    target = spellTarget;
    freezeRounds = freeze;
}

bool HeroCard::canBePlayedBy(const Player& player) const {
    const Hero* hero = dynamic_cast<const Hero*>(&player);

    if (hero == nullptr) {
        return false;
    }

    if (requiredClass.has_value()) {
        if (hero->getType().heroClass != requiredClass.value()) {
            return false;
        }
    }

    return true;
}

void HeroCard::applyEffect(Targetable& target) const {
    if (effect == HeroEffect::ATTACK || effect == HeroEffect::SPELL) {
        auto* monster = dynamic_cast<Monster*>(&target);
        if (effect == HeroEffect::SPELL && this->target == SpellTarget::BOSS_ONLY &&
            (!monster || !monster->isBoss())) {
            throw std::invalid_argument("This spell requires the boss");
        }
        target.takeDamage(value);
        if (effect == HeroEffect::SPELL && freezeRounds == 1 && monster && monster->isAlive()) {
            monster->freezeNextAttack();
        }

    } else if (effect == HeroEffect::HEAL || effect == HeroEffect::SHIELD) {
        Hero* hero = dynamic_cast<Hero*>(&target);

        if (hero == nullptr) {
            throw std::invalid_argument("This card requires a hero target");
        }

        if (!hero->isAlive()) {
            throw std::invalid_argument("Cannot target a defeated hero");
        }

        if (effect == HeroEffect::HEAL) {
            hero->heal(value);
        } else {
            hero->addShield(value);
        }

    } else if (effect == HeroEffect::FIREBALL) {
        Monster* monster = dynamic_cast<Monster*>(&target);

        if (monster == nullptr) {
            throw std::invalid_argument("Fireball must target a monster");
        }

        if (!monster->isAlive()) {
            throw std::invalid_argument("Cannot target a defeated monster");
        }

        monster->takeDamage(value);

        if (monster->isAlive()) {
            monster->applyBurn();
        }

    } else {
        throw std::logic_error("This hero effect is not implemented yet");
    }
}

void DMCard::applyEffect(Targetable& target) const {
    bool supportedEffect = effect == DMEffect::ATTACK || effect == DMEffect::WEAKNESS || effect == DMEffect::MANA_TRAP || effect == DMEffect::DISCARD_TRAP || effect == DMEffect::MANA_DRAIN_TRAP;

    if (!supportedEffect) {
        throw std::logic_error("This DM effect is not implemented yet");
    }

    Hero* hero = dynamic_cast<Hero*>(&target);

    if (hero == nullptr) {
        throw std::invalid_argument("This card must target a hero");
    }

    if (!hero->isAlive()) {
        throw std::invalid_argument("Cannot target a defeated hero");
    }

    if (effect == DMEffect::ATTACK) {
        hero->takeDamage(value);

    } else if (effect == DMEffect::WEAKNESS) {
        hero->applyWeakness(value);

    } else if (effect == DMEffect::MANA_TRAP) {
        hero->armManaTrap(TrapEffect::DAMAGE);

    } else if (effect == DMEffect::DISCARD_TRAP) {
        hero->armManaTrap(TrapEffect::DISCARD);

    } else if (effect == DMEffect::MANA_DRAIN_TRAP) {
        hero->armManaTrap(TrapEffect::MANA_DRAIN);
    }
}

DMCard::DMCard(std::string n, int c, DMEffect e, int v)
    : Card(std::move(n), c), effect(e), value(v){
        if (v < 0){
            throw std::invalid_argument("negative effect value");
        }
    }

bool DMCard::canBePlayedBy(const Player& p) const {
    return dynamic_cast<const DungeonMaster*>(&p) != nullptr;
}

// void DMCard::applyEffect(Targetable& target) const{
//      if (effect == DMEffect::ATTACK) {
//         Hero* hero = dynamic_cast<Hero*>(&target);

//         if (hero == nullptr) {
//             throw std::invalid_argument("Monster attacks must target a hero");
//         }

//         if (!hero->isAlive()) {
//             throw std::invalid_argument("Cannot attack a defeated hero");
//         }

//         hero->takeDamage(value);
//     } else {
//         throw std::logic_error("This DM effect is not implemented yet");
//     }
// }

const std::string& Card::getName() const{
    return name;
}

int Card::getManaCost() const{
    return manaCost;
}

HeroEffect HeroCard::getEffect() const {
    return effect;
}

int HeroCard::getValue() const {
    return value;
}

DMEffect DMCard::getEffect() const {
    return effect;
}

int DMCard::getValue() const {
    return value;
}

SupportTarget HeroCard::getSupportTarget() const {
    return supportTarget;
}
