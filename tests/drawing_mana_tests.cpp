#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"
#include <iostream>
#include <stdexcept>
#include <string>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid_argument");
}
std::unique_ptr<HeroCard> strike(const char* name, int cost) {
    return std::make_unique<HeroCard>(name, cost, HeroEffect::ATTACK, 1);
}
struct Battle {
    Match match{"drawing-mana", 5};
    Hero* hero;
    DungeonMaster* dm;
    Monster* monster;
    explicit Battle(int mana) {
        auto h = std::make_unique<Hero>("h", "Hero", HeroType{HeroClass::WARRIOR, 30}, mana);
        hero = h.get();
        auto d = std::make_unique<DungeonMaster>("dm", "DM", 3);
        dm = d.get();
        auto m = std::make_unique<Monster>("Goblin", 100);
        monster = m.get();
        match.addPlayer(std::move(h));
        match.addPlayer(std::move(d));
        match.addMonster(std::move(m));
    }
};
void drawing() {
    Hero hero("h", "Hero", {HeroClass::WARRIOR, 30}, 3);
    require(hero.drawCard() == nullptr && hero.getHandSize() == 0, "Empty draw must leave hand empty");
    auto first = strike("First", 1);
    auto second = strike("Second", 2);
    const Card* firstView = first.get();
    const Card* secondView = second.get();
    hero.addToDeck(std::move(first));
    hero.addToDeck(std::move(second));
    require(hero.drawCard() == firstView, "First card must be drawn first");
    require(hero.drawCard() == secondView, "Second draw must return next card");
    require(hero.getHandSize() == 2 && &hero.getHandCard(0) == firstView &&
            &hero.getHandCard(1) == secondView, "Draw must preserve objects and hand order");
    require(dynamic_cast<const HeroCard*>(&hero.getHandCard(0)) != nullptr, "Draw must preserve card type");
    require(hero.drawCard() == nullptr && hero.drawCard() == nullptr, "Exhausted deck must keep returning null");
    require(hero.getHandSize() == 2 && hero.getDiscardSize() == 0 && hero.getMana() == 3,
            "Empty draw must preserve hand, discard and mana");
}
void manaBoundaries() {
    Hero hero("h", "Hero", {HeroClass::WARRIOR, 30}, 3);
    require(hero.getMana() == 3 && hero.getMaxMana() == 3, "Initial mana must equal maximum");
    require(!hero.canPay(-1) && hero.canPay(0) && hero.canPay(3) && !hero.canPay(4), "Affordability boundaries");
    require(hero.getMana() == 3, "Affordability query must not spend mana");
    hero.refillMana(); hero.refillMana();
    require(hero.getMana() == 3, "Refill must not accumulate mana");
    Hero zero("z", "Zero", {HeroClass::WARRIOR, 30}, 0);
    zero.refillMana();
    require(zero.getMana() == 0 && zero.canPay(0) && !zero.canPay(1), "Zero mana boundaries");
    rejects([] { Hero invalid("h", "Hero", {HeroClass::WARRIOR, 30}, -1); });
    rejects([] { strike("Invalid", -1); });
}
void spendingAndTurns() {
    Battle b(3);
    b.hero->addToDeck(strike("Exact cost", 3));
    b.hero->addToDeck(strike("Free", 0));
    b.dm->addToDeck(std::make_unique<DMCard>("Summon", 1, DMEffect::SUMMON, 1));
    b.match.start();
    require(b.hero->getHandSize() == 1 && b.dm->getHandSize() == 0, "Only active player draws at start");
    b.match.apply(b.hero->getHandCard(0), *b.monster);
    require(b.hero->getMana() == 0 && b.hero->getHandSize() == 0 && b.hero->getDiscardSize() == 1,
            "Successful play spends exact cost and discards once");
    b.match.endTurn(*b.hero);
    require(b.hero->getMana() == 0 && b.dm->getHandSize() == 1, "DM turn draws without refilling inactive hero");
    b.match.endTurn(*b.dm);
    require(b.hero->getMana() == 3 && b.hero->getHandSize() == 1, "Next hero turn refills and draws one");
    b.match.apply(b.hero->getHandCard(0), *b.monster);
    require(b.hero->getMana() == 3, "Free card must not spend mana");
    b.match.endTurn(*b.hero); b.match.endTurn(*b.dm);
    require(b.hero->getHandSize() == 0 && b.hero->getDiscardSize() == 2 && b.hero->getMana() == 3,
            "Empty deck on turn start must not recycle discard or stop refill");
}
void rejectedPlay() {
    Battle b(2);
    b.hero->addToDeck(strike("Too expensive", 3));
    b.match.start();
    const Card* card = &b.hero->getHandCard(0);
    const int hp = b.monster->getHealth();
    require(!b.match.validate(*b.hero, *card, *b.monster).valid, "Unaffordable card must fail validation");
    rejects([&] { b.match.apply(*card, *b.monster); });
    require(b.hero->getMana() == 2 && b.hero->getHandSize() == 1 && b.hero->getDiscardSize() == 0 &&
            &b.hero->getHandCard(0) == card && b.monster->getHealth() == hp,
            "Rejected play must preserve mana, card and target");
    Battle invalidTarget(3);
    invalidTarget.hero->addToDeck(strike("Strike", 1));
    invalidTarget.match.start();
    rejects([&] { invalidTarget.match.apply(invalidTarget.hero->getHandCard(0), *invalidTarget.hero); });
    require(invalidTarget.hero->getMana() == 3 && invalidTarget.hero->getDiscardSize() == 0,
            "Wrong target must not charge mana");
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected case name");
        const std::string name = argv[1];
        if (name == "drawing") drawing();
        else if (name == "mana_boundaries") manaBoundaries();
        else if (name == "spending_turns") spendingAndTurns();
        else if (name == "rejected_play") rejectedPlay();
        else throw std::runtime_error("Unknown case");
        std::cout << name << " passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
