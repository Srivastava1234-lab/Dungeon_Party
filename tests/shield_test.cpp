#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

#include "Match.h"
#include "HeroCard.h"

int main() {
    Match match("shield-test", 10);

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

    for (int i = 0; i < 10; i++) {
        hero->addToDeck(
            std::make_unique<HeroCard>(
                "Shield", 1, HeroEffect::SHIELD, 5
            )
        );
    }

    match.addPlayer(std::move(hero));
    match.addPlayer(std::move(dm));
    match.addMonster(std::move(monster));

    match.start();

    std::size_t startingHand = heroView->getHandSize();

    // Play Shield on the hero.
    match.apply(heroView->getHandCard(0), *heroView);

    assert(heroView->getShield() == 5);
    assert(heroView->getHealth() == 30);
    assert(heroView->getMana() == 2);
    assert(heroView->getHandSize() == startingHand - 1);
    assert(heroView->getCardsPlayedThisTurn() == 1);

    std::cout << "Shield applied and card cost paid.\n";

    // Shield absorbs the entire hit.
    heroView->takeDamage(3);

    assert(heroView->getShield() == 2);
    assert(heroView->getHealth() == 30);

    std::cout << "Shield absorbs damage before health.\n";

    // Remaining shield absorbs 2; the other 3 damage hits HP.
    heroView->takeDamage(5);

    assert(heroView->getShield() == 0);
    assert(heroView->getHealth() == 27);

    std::cout << "Excess damage reduces health.\n";

    // Two Shield cards stack to 10.
    match.apply(heroView->getHandCard(0), *heroView);
    match.apply(heroView->getHandCard(0), *heroView);

    assert(heroView->getShield() == 10);
    assert(heroView->getMana() == 0);

    // An exact hit removes shield without damaging HP.
    heroView->takeDamage(10);

    assert(heroView->getShield() == 0);
    assert(heroView->getHealth() == 27);

    std::cout << "Shields stack and absorb an exact hit.\n";

    // Shield must not consume Weakness.
    heroView->applyWeakness(2);

    HeroCard shieldCard("Shield", 1, HeroEffect::SHIELD, 5);
    shieldCard.applyEffect(*heroView);

    assert(heroView->getShield() == 5);
    assert(heroView->getAttackReduction() == 2);

    std::cout << "Shield does not consume Weakness.\n";
    std::cout << "All Shield tests passed!\n";

    return 0;
}