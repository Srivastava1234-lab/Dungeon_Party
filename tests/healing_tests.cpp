#include "Hero.h"
#include "HeroCard.h"
#include "Monster.h"
#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void healthIs(const Hero& hero, int expected) {
    if (hero.getHealth() != expected)
        throw std::runtime_error("Expected HP " + std::to_string(expected) +
                                 ", actual HP " + std::to_string(hero.getHealth()));
}
template<class Exception, class F> void rejects(F action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown");
}

void run(const std::string& name) {
    // The current enum is HEALER. The team's announced CLERK rename is not in main yet.
    Hero hero("support", "Support", {HeroClass::HEALER, 30}, 10);
    if (name == "amount") {
        hero.takeDamage(10); hero.heal(6); healthIs(hero, 26);
    } else if (name == "max_cap") {
        hero.takeDamage(5); hero.heal(10); healthIs(hero, 30);
    } else if (name == "full_health") {
        hero.heal(5); healthIs(hero, 30);
    } else if (name == "nonpositive") {
        hero.takeDamage(10);
        hero.heal(0); healthIs(hero, 20);
        hero.heal(-5); healthIs(hero, 20);
        hero.heal(INT_MIN); healthIs(hero, 20);
    } else if (name == "large_amount") {
        hero.takeDamage(5); hero.heal(INT_MAX); healthIs(hero, 30);
        // A correct implementation must also avoid signed integer overflow.
        Hero large("large", "Large", {HeroClass::HEALER, INT_MAX}, 10);
        large.takeDamage(10); large.heal(INT_MAX); healthIs(large, INT_MAX);
    } else if (name == "card_effect") {
        hero.takeDamage(10);
        HeroCard card("Heal", 2, HeroEffect::HEAL, 5);
        check(card.canBePlayedBy(hero), "Support class must accept a healing card");
        card.applyEffect(hero); healthIs(hero, 25);
    } else if (name == "role_and_target") {
        Hero warrior("warrior", "Warrior", {HeroClass::WARRIOR, 30}, 10);
        rejects<std::invalid_argument>([&] {
            warrior.addToDeck(std::make_unique<HeroCard>("Heal", 2, HeroEffect::HEAL, 5));
        });
        check(warrior.getHandSize() == 0 && warrior.getMana() == 10,
              "Rejected healing card changed warrior state");
        Monster monster("Goblin", 20);
        HeroCard card("Heal", 2, HeroEffect::HEAL, 5);
        rejects<std::invalid_argument>([&] { card.applyEffect(monster); });
        check(monster.getHealth() == 20, "Healing a monster must not change its HP");
    } else throw std::runtime_error("Unknown case: " + name);
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: healing_tests <case>");
        run(argv[1]); std::cout << "PASS: " << argv[1] << '\n'; return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
