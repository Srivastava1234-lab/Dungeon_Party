#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

#include "Match.h"
#include "HeroCard.h"

int main() {
    Match match("support-test", 10);

    auto warrior = std::make_unique<Hero>(
        "warrior", "Warrior",
        HeroType{HeroClass::WARRIOR, 30}, 10
    );

    auto healer = std::make_unique<Hero>(
        "healer", "Healer",
        HeroType{HeroClass::HEALER, 30}, 10
    );

    Hero* warriorView = warrior.get();
    Hero* healerView = healer.get();

    warrior->addToDeck(
        std::make_unique<HeroCard>(
            "Basic Heal", 1, HeroEffect::HEAL, 5
        )
    );

    // Greater Heal must be restricted to Healers.
    HeroCard greaterHeal(
        "Greater Heal", 2, HeroEffect::HEAL, 10,
        HeroClass::HEALER,
        SupportTarget::SELF_OR_ALLY
    );

    assert(!greaterHeal.canBePlayedBy(*warrior));
    assert(greaterHeal.canBePlayedBy(*healer));

    // Give the Healer two copies: one for an ally, one for self.
    for (int i = 0; i < 2; i++) {
        healer->addToDeck(
            std::make_unique<HeroCard>(
                "Greater Heal", 2, HeroEffect::HEAL, 10,
                HeroClass::HEALER,
                SupportTarget::SELF_OR_ALLY
            )
        );
    }

    match.addPlayer(std::move(warrior));
    match.addPlayer(std::move(healer));

    match.addPlayer(
        std::make_unique<DungeonMaster>(
            "dm", "Dungeon Master", 3, false
        )
    );

    match.addMonster(
        std::make_unique<Monster>("Goblin", 100)
    );

    match.start();

    warriorView->takeDamage(15);
    healerView->takeDamage(15);

    // Basic Heal cannot target another hero.
    const Card& basicHeal = warriorView->getHandCard(0);

    assert(!match.validate(
        *warriorView, basicHeal, *healerView
    ).valid);

    bool rejected = false;

    try {
        match.apply(basicHeal, *healerView);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(warriorView->getMana() == 10);
    assert(warriorView->getHandSize() == 1);
    assert(healerView->getHealth() == 15);

    // Basic Heal restores exactly 5 HP to the caster.
    match.apply(basicHeal, *warriorView);

    assert(warriorView->getHealth() == 20);
    assert(warriorView->getMana() == 9);

    std::cout << "Basic Heal works on self only.\n";

    match.endTurn(*warriorView);

    // Greater Heal restores 10 HP to an ally.
    match.apply(
        healerView->getHandCard(0), *warriorView
    );

    assert(warriorView->getHealth() == 30);
    assert(healerView->getMana() == 8);

    // Greater Heal can also target the Healer.
    match.apply(
        healerView->getHandCard(0), *healerView
    );

    assert(healerView->getHealth() == 25);
    assert(healerView->getMana() == 6);

    std::cout << "Greater Heal works on self and allies.\n";
    std::cout << "All support card tests passed!\n";

    return 0;
}