#include "Match.h"
#include "HeroCard.h"
#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class Exception, class F> void rejects(F action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown");
}
struct Battle {
    Match match{"combat", 2};
    Hero* hero;
    DungeonMaster* dm;
    Monster* monster;
    Battle(int damage, bool extraMonster = false) {
        auto h = std::make_unique<Hero>("h", "Hero", HeroType{HeroClass::WARRIOR, 30}, 10);
        hero = h.get();
        h->addToDeck(std::make_unique<HeroCard>("Strike", 1, HeroEffect::ATTACK, damage));
        auto d = std::make_unique<DungeonMaster>("dm", "DM", 3); dm = d.get();
        auto m = std::make_unique<Monster>("Goblin", 20); monster = m.get();
        match.addPlayer(std::move(h)); match.addPlayer(std::move(d)); match.addMonster(std::move(m));
        if (extraMonster) match.addMonster(std::make_unique<Monster>("Other", 10));
        match.start();
    }
};
void attackDamage() {
    Battle b(6);
    b.match.apply(b.hero->getHandCard(0), *b.monster);
    check(b.monster->getHealth() == 14, "Attack must deal exactly its damage");
    check(!b.match.isFinished(), "Surviving monster must not end match");
    Monster monster("Target", 20);
    monster.takeDamage(0); monster.takeDamage(-4);
    check(monster.getHealth() == 20, "Nonpositive damage must not change HP");
    monster.takeDamage(INT_MAX);
    check(monster.getHealth() == 0 && !monster.isAlive(), "Overkill must clamp monster HP to zero");
}
void invalidTargets() {
    Battle b(6);
    Monster outsider("Outside match", 20);
    const Card& card = b.hero->getHandCard(0);
    rejects<std::invalid_argument>([&] { b.match.apply(card, outsider); });
    rejects<std::invalid_argument>([&] { b.match.apply(card, *b.hero); });
    b.monster->takeDamage(20);
    rejects<std::invalid_argument>([&] { b.match.apply(card, *b.monster); });
    check(outsider.getHealth() == 20 && b.hero->getHealth() == 30 && b.hero->getHandSize() == 1 &&
          b.hero->getMana() == 10, "Rejected attack must preserve target and player state");
}
void monsterDamage() {
    Hero hero("h", "Hero", {HeroClass::WARRIOR, 30}, 3);
    Monster monster("Goblin", 20);
    HeroCard damage("Damage", 0, HeroEffect::ATTACK, 6);
    monster.attack(damage, hero);
    check(hero.getHealth() == 24, "Monster attack: 30 HP minus 6 damage must equal 24");
    monster.takeDamage(20);
    rejects<std::logic_error>([&] { monster.attack(damage, hero); });
    check(hero.getHealth() == 24, "Defeated monster must not deal damage");
    hero.takeDamage(INT_MAX);
    check(hero.getHealth() == 0 && !hero.isAlive(), "Lethal damage must reduce hero HP to zero");
}
void victory() {
    Battle remaining(20, true);
    remaining.match.apply(remaining.hero->getHandCard(0), *remaining.monster);
    check(!remaining.match.isFinished(), "One remaining living monster prevents victory");
    Battle last(25);
    last.match.apply(last.hero->getHandCard(0), *last.monster);
    check(last.match.isFinished() && last.match.getWinner() == Side::HEROES, "Last monster defeat wins match");
    check(last.match.getActivePlayer() == nullptr, "Finished match has no active player");
    HeroCard extra("Extra", 0, HeroEffect::ATTACK, 1);
    rejects<std::logic_error>([&] { last.match.apply(extra, *last.monster); });
}
void roundLimit() {
    Battle b(1);
    b.match.endTurn(*b.hero); b.match.endTurn(*b.dm);
    check(b.match.getRound() == 2 && !b.match.isFinished(), "Match must continue before final round ends");
    b.match.endTurn(*b.hero);
    check(!b.match.isFinished(), "Final hero turn alone must not end match");
    b.match.endTurn(*b.dm);
    check(b.match.isFinished() && b.match.getWinner() == Side::DUNGEON_MASTER, "Round limit awards DM victory");
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected case name");
        const std::string name = argv[1];
        if (name == "attack_damage") attackDamage();
        else if (name == "invalid_targets") invalidTargets();
        else if (name == "monster_damage") monsterDamage();
        else if (name == "victory") victory();
        else if (name == "round_limit") roundLimit();
        else throw std::runtime_error("Unknown case");
        std::cout << name << " passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
