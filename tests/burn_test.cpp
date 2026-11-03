#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"

int main() {
    Match match("burn-test", 10);

    std::unique_ptr<Hero> hero = std::make_unique<Hero>(
        "hero-1",
        "Warrior",
        HeroType{HeroClass::WARRIOR, 30},
        3
    );

    std::unique_ptr<DungeonMaster> dm =
        std::make_unique<DungeonMaster>(
            "dm-1", "Dungeon Master", 3, false
        );

    std::unique_ptr<Monster> monster =
        std::make_unique<Monster>("Goblin", 100);

    Hero* heroView = hero.get();
    DungeonMaster* dmView = dm.get();
    Monster* monsterView = monster.get();

    hero->addToDeck(
        std::make_unique<HeroCard>(
            "Fireball", 2, HeroEffect::FIREBALL, 10
        )
    );

    dm->addToDeck(
        std::make_unique<DMCard>(
            "Claw", 1, DMEffect::ATTACK, 5
        )
    );

    match.addPlayer(std::move(hero));
    match.addPlayer(std::move(dm));
    match.addMonster(std::move(monster));

    match.start();

    // Fireball deals immediate damage and starts Burn.
    match.apply(heroView->getHandCard(0), *monsterView);

    assert(monsterView->getHealth() == 90);
    assert(monsterView->getBurnTurnsRemaining() == 2);
    assert(heroView->getMana() == 1);
    assert(heroView->getHandSize() == 0);

    std::cout << "Fireball deals 10 damage and applies Burn.\n";

    // Ending the hero turn must not trigger Burn.
    match.endTurn(*heroView);

    assert(monsterView->getHealth() == 90);
    assert(monsterView->getBurnTurnsRemaining() == 2);

    // Ending the DM turn triggers the first tick.
    match.endTurn(*dmView);

    assert(monsterView->getHealth() == 88);
    assert(monsterView->getBurnTurnsRemaining() == 1);

    std::cout << "First monster-action phase deals 2 Burn damage.\n";

    // Second tick.
    match.endTurn(*heroView);
    match.endTurn(*dmView);

    assert(monsterView->getHealth() == 86);
    assert(monsterView->getBurnTurnsRemaining() == 0);

    std::cout << "Second tick deals 2 damage and Burn expires.\n";

    // Burn must not trigger a third time.
    match.endTurn(*heroView);
    match.endTurn(*dmView);

    assert(monsterView->getHealth() == 86);

    std::cout << "Expired Burn causes no more damage.\n";

    // Applying Burn again refreshes its duration.
    monsterView->applyBurn();
    monsterView->processBurn();

    assert(monsterView->getBurnTurnsRemaining() == 1);

    monsterView->applyBurn();

    assert(monsterView->getBurnTurnsRemaining() == 2);

    std::cout << "Reapplying Burn refreshes the duration.\n";

    // Leave the monster at 2 HP.
    monsterView->takeDamage(monsterView->getHealth() - 2);

    // Queue an attack that must not execute if Burn kills it.
    match.endTurn(*heroView);

    match.queueMonsterAttack(
        *dmView,
        *monsterView,
        dmView->getHandCard(0),
        *heroView
    );

    match.endTurn(*dmView);

    assert(monsterView->getHealth() == 0);
    assert(heroView->getHealth() == 30);
    assert(match.getWinner() == Side::HEROES);

    std::cout << "Burn kills the monster before its queued attack.\n";
    std::cout << "All Burn tests passed!\n";

    return 0;
}