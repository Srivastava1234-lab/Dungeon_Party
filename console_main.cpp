#include <iostream>
#include <memory>
#include "Match.h"
#include "HeroCard.h"
int main() {
    Match match("match-1", 20);
    auto hero = std::make_unique<Hero>("hero-1", "Warrior", HeroType{HeroClass::WARRIOR, 30}, 3);
    auto dm = std::make_unique<DungeonMaster>("dm-1", "Dungeon Master", 3, false);
    auto monster = std::make_unique<Monster>("Goblin", 20);
    Hero* heroView = hero.get();
    Monster* monsterView = monster.get();
    for (int i = 0; i < 10; ++i)
        hero->addToDeck(std::make_unique<HeroCard>("Strike", 1, HeroEffect::ATTACK, 6));
    match.addPlayer(std::move(hero)); match.addPlayer(std::move(dm));
    match.addMonster(std::move(monster)); match.start();
    char command;
    while (!match.isFinished()) {
        auto* active = match.getActivePlayer();
        std::cout << "\nRound " << match.getRound() << " | " << active->getName()
                  << " | Hero HP " << heroView->getHealth() << " | Monster HP " << monsterView->getHealth()
                  << " | Mana " << active->getMana() << " | Hand " << active->getHandSize()
                  << "\na = play first card, e = end turn, q = quit\n> ";
        if (!(std::cin >> command) || command == 'q') break;
        try {
            if (command == 'e') match.endTurn(*active);
            else if (command == 'a' && active->getHandSize()) match.apply(active->getHandCard(0), *monsterView);
            else std::cout << "No action available.\n";
        } catch (const std::exception& e) { std::cout << e.what() << '\n'; }
    }
    if (match.isFinished()) std::cout << (match.getWinner() == Side::HEROES ? "Heroes win!\n" : "DM wins!\n");
}
