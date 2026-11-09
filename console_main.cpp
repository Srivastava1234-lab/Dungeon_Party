#include <iostream>
#include <memory>
#include <exception>
#include <utility>

#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"

int main() {
    Match match("match-1", 20);

    // Create two heroes.
    std::unique_ptr<Hero> hero = std::make_unique<Hero>(
        "hero-1",
        "Warrior",
        HeroType{HeroClass::WARRIOR, 30},
        3
    );

    std::unique_ptr<Hero> secondHero = std::make_unique<Hero>(
        "hero-2",
        "Second Warrior",
        HeroType{HeroClass::WARRIOR, 30},
        3
    );

    // Create the DM and monster.
    std::unique_ptr<DungeonMaster> dm =
        std::make_unique<DungeonMaster>(
            "dm-1",
            "Dungeon Master",
            3,
            false
        );

    std::unique_ptr<Monster> monster =
        std::make_unique<Monster>("Goblin", 200);

    // Keep pointers for displaying health and choosing targets.
    Hero* heroView = hero.get();
    Hero* secondHeroView = secondHero.get();
    DungeonMaster* dmView = dm.get();
    Monster* monsterView = monster.get();

    // Give both heroes attack cards.
    for (int i = 0; i < 10; i++) {
        hero->addToDeck(
            std::make_unique<HeroCard>(
                "Strike",
                1,
                HeroEffect::ATTACK,
                6
            )
        );

        secondHero->addToDeck(
            std::make_unique<HeroCard>(
                "Strike",
                1,
                HeroEffect::ATTACK,
                6
            )
        );
    }

    // Give the DM monster attack cards.
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

    // Transfer ownership to the match.
    match.addPlayer(std::move(hero));
    match.addPlayer(std::move(secondHero));
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
        std::cout << "Warrior HP: "
                  << heroView->getHealth() << "\n";
        std::cout << "Second Warrior HP: "
                  << secondHeroView->getHealth() << "\n";
        std::cout << "Monster HP: "
                  << monsterView->getHealth() << "\n";
        std::cout << "Active player: "
                  << active->getName() << "\n";
        std::cout << "Mana: "
                  << active->getMana() << "\n";
        std::cout << "Cards in hand: "
                  << active->getHandSize() << "\n";

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

                        std::cout << active->getName()
                                  << " played Strike.\n";
                    } else if (match.getPhase() == Phase::DM_TURN) {
                        std::cout << "Choose a target:\n";
                        std::cout << "1 = Warrior\n";
                        std::cout << "2 = Second Warrior\n";
                        std::cout << "> ";

                        int targetChoice;

                        if (!(std::cin >> targetChoice)) {
                            break;
                        }

                        Hero* target = nullptr;

                        if (targetChoice == 1) {
                            target = heroView;
                        } else if (targetChoice == 2) {
                            target = secondHeroView;
                        } else {
                            std::cout << "Invalid target.\n";
                        }

                        if (target != nullptr) {
                            match.queueMonsterAttack(
                                *dmView,
                                *monsterView,
                                card,
                                *target
                            );

                            std::cout << "Monster attack queued against "
                                      << target->getName() << ".\n";
                            std::cout << "End the turn to execute it.\n";
                        }
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
        std::cout << "\nFinal Warrior HP: "
                  << heroView->getHealth() << "\n";
        std::cout << "Final Second Warrior HP: "
                  << secondHeroView->getHealth() << "\n";
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