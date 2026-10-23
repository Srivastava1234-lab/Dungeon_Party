#include "Card.h"
#include "HeroCard.h"
#include "DMCard.h"
#include "Hero.h"
#include "DungeonMaster.h"
#include <stdexcept>
#include <utility>

Card::Card(const std::string& n, int cost)
    : name(n),
    manaCost(std::max(0, cost)){
        if (cost < 0)
            throw std::invalid_argument("negative mana cost");
    }

HeroCard::HeroCard(std::string n, int c, HeroEffect e, int v)        : Card(std::move(n), c), effect(e), value(v){
        if (v < 0){
            throw std::invalid_argument("negavtive effect value");
        }
    }

bool HeroCard::canBePlayedBy(const Player& p) const {
    const auto* hero = dynamic_cast<const Hero*>(&p);
    return hero && (effect != HeroEffect::HEAL || hero->getType().heroClass == HeroClass::HEALER);
}

void HeroCard::applyEffect(Targetable& t) const {
    if (effect == HeroEffect::ATTACK || effect == HeroEffect::SPELL){
        t.takeDamage(value);
    } else if(effect == HeroEffect::HEAL){
        auto* hero = dynamic_cast<Hero*>(&t);
        if(!hero){
            throw std::invalid_argument("healing requires a hero target");
        }
        hero->heal(value);
    } else {
        throw std::logic_error("Buff rules are not implemented yet");
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

void DMCard::applyEffect(Targetable&) const{
    throw std::logic_error("DM effect is not done yet");
}

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






