#include <iostream>
#include "include/Game.h"
#include "include/Hero.h"
#include "include/Card.h"   

void printState(const Game& game) {
    std::cout << "\nRound: " << game.getRound() << "\n";
    std::cout << "Hero HP: " << game.getHero().getHealth() << "\n";
    std::cout << "Monster HP: " << game.getBoss().getHealth() << "\n";

    if (game.getPhase() == Phase::HERO_TURN) {
        std::cout << "Hero turn\n";
    } else {
        std::cout << "Monster turn\n";
    }
}

int main() {
    Card heroCard("Strike", 1, 6);
Card monsterCard("Claw", 1, 5);

std::cout << heroCard.getName()
          << " | Mana: " << heroCard.getManaCost()
          << " | Damage: " << heroCard.getDamage() << "\n";

std::cout << monsterCard.getName()
          << " | Mana: " << monsterCard.getManaCost()
          << " | Damage: " << monsterCard.getDamage() << "\n";
    Game game;
    char choice;

    while (true) {
        printState(game);

        std::cout << "e = end turn, q = quit\n";
        std::cout << "> ";

        if (!(std::cin >> choice)) {
            break;
        }

        if (choice == 'q') {
            break;
        } else if (choice == 'e') {
            game.endTurn();
        } else {
            std::cout << "Invalid choice.\n";
        }
    }

    return 0;
}