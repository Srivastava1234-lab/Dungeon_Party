#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"

int main() {
    Match match("weakness-test", 10);

    std::unique_ptr<Hero> hero = std::make_unique<Hero>(
        "hero-1",
        "Healer",
        HeroType{HeroClass::HEALER, 30},
        10
    );

    std::unique_ptr<DungeonMaster> dm =
        std::make_unique<DungeonMaster>(
            "dm-1",
            "Dungeon Master",
            10,
            false
        );

    std::unique_ptr<Monster> monster =
        std::make_unique<Monster>("Goblin", 100);

    Hero* heroView = hero.get();
    DungeonMaster* dmView = dm.get();
    Monster* monsterView = monster.get();

    hero->addToDeck(
        std::make_unique<HeroCard>(
            "Heal", 1, HeroEffect::HEAL, 5
        )
    );

    for (int i = 0; i < 9; i++) {
        hero->addToDeck(
            std::make_unique<HeroCard>(
                "Strike", 1, HeroEffect::ATTACK, 6
            )
        );
    }

    dm->addToDeck(
        std::make_unique<DMCard>(
            "Weakness", 1, DMEffect::WEAKNESS, 2
        )
    );

    match.addPlayer(std::move(hero));
    match.addPlayer(std::move(dm));
    match.addMonster(std::move(monster));

    match.start();

    // Move to the DM's turn and play Weakness.
    match.endTurn(*heroView);

    match.apply(dmView->getHandCard(0), *heroView);

    assert(heroView->getAttackReduction() == 2);
    assert(dmView->getMana() == 9);
    assert(dmView->getHandSize() == 0);

    std::cout << "Weakness applied and mana paid.\n";

    match.endTurn(*dmView);

    // Healing must not consume Weakness.
    heroView->takeDamage(5);
    match.apply(heroView->getHandCard(0), *heroView);

    assert(heroView->getHealth() == 30);
    assert(heroView->getAttackReduction() == 2);

    std::cout << "Healing does not consume Weakness.\n";

    // First Strike: 6 - 2 = 4 damage.
    match.apply(heroView->getHandCard(0), *monsterView);

    assert(monsterView->getHealth() == 96);
    assert(heroView->getAttackReduction() == 0);

    std::cout << "First attack deals 4 damage.\n";

    // Second Strike: normal 6 damage.
    match.apply(heroView->getHandCard(0), *monsterView);

    assert(monsterView->getHealth() == 90);

    std::cout << "Second attack deals 6 damage.\n";
    std::cout << "All Weakness tests passed!\n";

    return 0;
}