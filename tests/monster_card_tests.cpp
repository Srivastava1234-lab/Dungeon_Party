#include "Match.h"
#include "DMCard.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class Exception, class F> void rejects(F action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown");
}

struct Battle {
    Match match{"monster-cards", 5};
    Hero* hero;
    Hero* otherHero = nullptr;
    DungeonMaster* dm;
    Monster* monster;
    explicit Battle(int mana = 10, bool secondHero = false) {
        auto h = std::make_unique<Hero>("hero", "Warrior", HeroType{HeroClass::WARRIOR, 30}, 10);
        hero = h.get(); match.addPlayer(std::move(h));
        if (secondHero) {
            auto other = std::make_unique<Hero>("other", "Other", HeroType{HeroClass::WARRIOR, 30}, 10);
            otherHero = other.get(); match.addPlayer(std::move(other));
        }
        auto d = std::make_unique<DungeonMaster>("dm", "DM", mana);
        dm = d.get();
        for (int i = 0; i < 4; ++i) {
            d->addToDeck(std::make_unique<DMCard>("Claw", 2, DMEffect::ATTACK, 6));
            d->drawCard(); // Explicit test setup: four independent cards in hand.
        }
        match.addPlayer(std::move(d));
        auto m = std::make_unique<Monster>("Goblin", 20);
        monster = m.get(); match.addMonster(std::move(m));
        match.addMonster(std::make_unique<Monster>("Reserve", 20));
        match.start();
    }
    void dmTurn() {
        while (match.getPhase() == Phase::HERO_TURN)
            match.endTurn(*match.getActivePlayer());
        check(match.getActivePlayer() == dm, "Expected the DM turn");
    }
    void queue() { match.queueMonsterAttack(*dm, *monster, dm->getHandCard(0), *hero); }
    void unchanged(int mana, std::size_t hand, std::size_t discard, int played) {
        check(dm->getMana() == mana && dm->getHandSize() == hand &&
              dm->getDiscardSize() == discard && dm->getCardsPlayedThisTurn() == played &&
              hero->getHealth() == 30, "Rejected order changed player or target state");
    }
};

// Observe real DM attack effects without replacing their damage implementation.
struct ObservedAttack : DMCard {
    std::vector<int>& events;
    int id;
    ObservedAttack(std::vector<int>& log, int token, int damage = 1)
        : DMCard("Observed attack", 0, DMEffect::ATTACK, damage), events(log), id(token) {}
    void applyEffect(Targetable& target) const override {
        events.push_back(id);
        DMCard::applyEffect(target);
    }
};
const Card& addObserved(Battle& b, std::vector<int>& log, int id, int damage = 1) {
    b.dm->addToDeck(std::make_unique<ObservedAttack>(log, id, damage));
    return *b.dm->drawCard();
}

void run(const std::string& name) {
    if (name == "deferred_damage") {
        Battle b; b.dmTurn();
        const Card* card = &b.dm->getHandCard(0);
        b.queue();
        check(b.hero->getHealth() == 30, "Damage must wait until the DM ends the turn");
        check(b.dm->getMana() == 8 && b.dm->getHandSize() == 3 &&
              b.dm->getDiscardSize() == 1 && b.dm->getCardsPlayedThisTurn() == 1,
              "An accepted order must charge and discard exactly once");
        rejects<std::invalid_argument>([&] {
            b.match.queueMonsterAttack(*b.dm, *b.monster, *card, *b.hero);
        });
        b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 24 && b.dm->getMana() == 8 &&
              b.dm->getDiscardSize() == 1, "Execution must deal six damage without charging again");
    } else if (name == "order") {
        std::vector<int> events; Battle b; b.dmTurn();
        const Card& first = addObserved(b, events, 1);
        const Card& second = addObserved(b, events, 2);
        b.match.queueMonsterAttack(*b.dm, *b.monster, second, *b.hero);
        b.match.queueMonsterAttack(*b.dm, *b.monster, first, *b.hero);
        check(events.empty(), "Effects ran while being queued");
        b.match.endTurn(*b.dm);
        check(events == std::vector<int>({2, 1}) && b.hero->getHealth() == 28,
              "Effects must execute in the selected order");
    } else if (name == "mana") {
        Battle b(1); b.dmTurn();
        rejects<std::invalid_argument>([&] { b.queue(); }); b.unchanged(1, 4, 0, 0);
        b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 30, "Rejected order must not enter the queue");
    } else if (name == "turn_authority") {
        Battle b;
        rejects<std::invalid_argument>([&] { b.queue(); }); b.unchanged(10, 4, 0, 0);
        b.dmTurn(); DungeonMaster outsider("outsider", "Outside DM", 10);
        rejects<std::invalid_argument>([&] {
            b.match.queueMonsterAttack(outsider, *b.monster, b.dm->getHandCard(0), *b.hero);
        });
        b.unchanged(10, 4, 0, 0); b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 30, "Unauthorized order must not execute");
    } else if (name == "invalid_targets") {
        Battle b; b.dmTurn();
        Monster outsideMonster("Outside", 20);
        Hero outsideHero("outside", "Outside hero", {HeroClass::WARRIOR, 30}, 10);
        const Card& card = b.dm->getHandCard(0);
        rejects<std::invalid_argument>([&] { b.match.queueMonsterAttack(*b.dm, outsideMonster, card, *b.hero); });
        rejects<std::invalid_argument>([&] { b.match.queueMonsterAttack(*b.dm, *b.monster, card, outsideHero); });
        b.monster->takeDamage(20);
        rejects<std::invalid_argument>([&] { b.queue(); }); b.unchanged(10, 4, 0, 0);
        b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 30 && outsideHero.getHealth() == 30,
              "Invalid targets must never receive damage");
        Battle deadTarget(10, true); deadTarget.dmTurn(); deadTarget.otherHero->takeDamage(30);
        rejects<std::invalid_argument>([&] {
            deadTarget.match.queueMonsterAttack(*deadTarget.dm, *deadTarget.monster,
                deadTarget.dm->getHandCard(0), *deadTarget.otherHero);
        });
        deadTarget.unchanged(10, 4, 0, 0);
    } else if (name == "invalid_cards") {
        Battle b; b.dmTurn(); DMCard outside("Outside", 0, DMEffect::ATTACK, 6);
        rejects<std::invalid_argument>([&] { b.match.queueMonsterAttack(*b.dm, *b.monster, outside, *b.hero); });
        b.dm->addToDeck(std::make_unique<DMCard>("Summon", 0, DMEffect::SUMMON, 1));
        const Card& summon = *b.dm->drawCard();
        rejects<std::invalid_argument>([&] { b.match.queueMonsterAttack(*b.dm, *b.monster, summon, *b.hero); });
        b.unchanged(10, 5, 0, 0); b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 30, "Invalid card must not enter the queue");
    } else if (name == "turn_limit") {
        Battle b; b.dmTurn(); b.queue(); b.queue(); b.queue();
        rejects<std::invalid_argument>([&] { b.queue(); });
        b.unchanged(4, 1, 3, 3); b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 12, "Only three accepted attacks may execute");
    } else if (name == "queue_cleared") {
        Battle b; b.dmTurn(); b.queue(); b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 24, "First round damage missing");
        b.dmTurn(); b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 24 && b.match.getRound() == 3,
              "Old attack orders must not repeat next round");
    } else if (name == "defeated_participants") {
        Battle b; b.dmTurn(); b.queue(); b.monster->takeDamage(20);
        b.match.endTurn(*b.dm);
        check(b.hero->getHealth() == 30, "A monster defeated after queuing must not attack");
        Battle target(10, true); target.dmTurn(); target.queue(); target.hero->takeDamage(30);
        target.match.endTurn(*target.dm);
        check(target.hero->getHealth() == 0 && target.otherHero->getHealth() == 30 &&
              !target.match.isFinished(), "An order targeting a defeated hero must be skipped");
    } else if (name == "match_end") {
        std::vector<int> events; Battle b; b.dmTurn();
        const Card& lethal = addObserved(b, events, 1, 30);
        const Card& later = addObserved(b, events, 2);
        b.match.queueMonsterAttack(*b.dm, *b.monster, lethal, *b.hero);
        b.match.queueMonsterAttack(*b.dm, *b.monster, later, *b.hero);
        b.match.endTurn(*b.dm);
        check(b.match.getWinner() == Side::DUNGEON_MASTER && b.hero->getHealth() == 0 &&
              events == std::vector<int>({1}), "Execution must stop when the match ends");
        rejects<std::logic_error>([&] { b.queue(); });
    } else throw std::runtime_error("Unknown case: " + name);
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: monster_card_tests <case>");
        run(argv[1]); std::cout << "PASS: " << argv[1] << '\n'; return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
