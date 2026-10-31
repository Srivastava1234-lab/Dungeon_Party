#include <iostream>
#include <memory>

#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"

int main() {
    Match match("match-1", 20);

    // Create the hero, DM and monster.
    std::unique_ptr<Hero> hero = std::make_unique<Hero>(
        "hero-1",
        "Warrior",
        HeroType{HeroClass::WARRIOR, 30},
        3
    );

    std::unique_ptr<DungeonMaster> dm =
        std::make_unique<DungeonMaster>(
            "dm-1",
            "Dungeon Master",
            3,
            false
        );

    std::unique_ptr<Monster> monster =
        std::make_unique<Monster>("Goblin", 20);

    // Keep pointers so the console can access these objects.
    Hero* heroView = hero.get();
    DungeonMaster* dmView = dm.get();
    Monster* monsterView = monster.get();

    // Add attack cards to the hero's deck.
    for (int i = 0; i < 10; i++) {
        hero->addToDeck(
            std::make_unique<HeroCard>(
                "Strike",
                1,
                HeroEffect::ATTACK,
                6
            )
        );
    }

    // Add monster attack cards to the DM's deck.
    for (int i = 0; i < 10; i++) {
        dm->addToDeck(
            std::make_unique<DMCard>(
                "Claw",
                1,
                DMEffect::ATTACK,
                5
            )
        );
    }

    // Transfer ownership of the characters to the match.
    match.addPlayer(std::move(hero));
    match.addPlayer(std::move(dm));
    match.addMonster(std::move(monster));

    match.start();

    char command;

    while (!match.isFinished()) {
        Player* active = match.getActivePlayer();

        if (active == nullptr) {
            std::cout << "No active player.\n";
            break;
        }

        std::cout << "\nRound: " << match.getRound() << "\n";
        std::cout << "Hero HP: " << heroView->getHealth() << "\n";
        std::cout << "Monster HP: " << monsterView->getHealth() << "\n";
        std::cout << "Active player: " << active->getName() << "\n";
        std::cout << "Mana: " << active->getMana() << "\n";
        std::cout << "Cards in hand: " << active->getHandSize() << "\n";

        if (active->getHandSize() > 0) {
            std::cout << "First card: "
                      << active->getHandCard(0).getName() << "\n";
        }

        std::cout << "\na = play first card\n";
        std::cout << "e = end turn\n";
        std::cout << "q = quit\n";
        std::cout << "> ";

        if (!(std::cin >> command)) {
            break;
        }

        if (command == 'q') {
            break;
        }

        try {
            if (command == 'e') {
                match.endTurn(*active);
            } else if (command == 'a') {
                if (active->getHandSize() == 0) {
                    std::cout << "No cards in hand.\n";
                } else {
                    const Card& card = active->getHandCard(0);

                    if (match.getPhase() == Phase::HERO_TURN) {
                        match.apply(card, *monsterView);

                        std::cout << "Hero played Strike.\n";
                    } else if (match.getPhase() == Phase::DM_TURN) {
                        match.queueMonsterAttack(
                            *dmView,
                            *monsterView,
                            card,
                            *heroView
                        );

                        std::cout << "Monster attack queued. "
                                  << "End the turn to execute it.\n";
                    }
                }
            } else {
                std::cout << "Invalid command.\n";
            }
        } catch (const std::exception& error) {
            std::cout << error.what() << "\n";
        }
    }

    if (match.isFinished()) {
        std::cout << "\nFinal hero HP: "
                  << heroView->getHealth() << "\n";

        std::cout << "Final monster HP: "
                  << monsterView->getHealth() << "\n";

        if (match.getWinner() == Side::HEROES) {
            std::cout << "Heroes win!\n";
        } else {
            std::cout << "DM wins!\n";
        }
    }

    return 0;
}