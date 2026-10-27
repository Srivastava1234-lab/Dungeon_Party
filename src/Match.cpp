#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

Match::Match(std::string id, int limit)
    : matchId(std::move(id)), maxRounds(limit) {
    if (maxRounds <= 0 || matchId.empty()) throw std::invalid_argument("Invalid match setup");
}

void Match::addPlayer(std::unique_ptr<Player> p) {
    if (started || !p) throw std::logic_error("Cannot add player");
    for (const auto& other : players)
        if (other->getPlayerId() == p->getPlayerId()) throw std::invalid_argument("Duplicate player ID");
    if (!dynamic_cast<Hero*>(p.get()) && !dynamic_cast<DungeonMaster*>(p.get()))
        throw std::invalid_argument("Unsupported player role");
    players.push_back(std::move(p));
}

void Match::addMonster(std::unique_ptr<Monster> m) {
    if (started || !m) throw std::logic_error("Configure monsters before start");
    monsters.push_back(std::move(m));
}

void Match::start() {
    if (started) throw std::logic_error("Match already started");
    heroes.clear(); dm = nullptr;
    for (const auto& p : players) {
        if (auto* h = dynamic_cast<Hero*>(p.get())) heroes.push_back(h);
        if (auto* d = dynamic_cast<DungeonMaster*>(p.get())) {
            if (dm) throw std::logic_error("Exactly one DM required");
            dm = d;
        }
    }
    if (heroes.empty() || heroes.size() > 6 || !dm || monsters.empty()) {
        throw std::logic_error("Need 1-6 heroes, exactly one DM and at least one monster");
    }
    started = true; round = 1; activeHero = 0; phase = Phase::HERO_TURN;
    beginTurn(*heroes.front());
}

void Match::beginTurn(Player& p) { p.refillMana(); p.cardsPlayedThisTurn = 0; p.drawCard(); }

Player* Match::getActivePlayer() const {
    if (!started || isFinished()) return nullptr;
    if (phase == Phase::HERO_TURN) return heroes.at(activeHero);
    if (phase == Phase::DM_TURN) return dm;
    return nullptr;
}

CheckResult Match::validate(const Player& p, const Card& c, const Targetable& t) const {
    auto fail = [](const char* s){ return CheckResult{false, std::string(s)}; };
    if (!started || isFinished()) return fail("Match is not active");
    if (getActivePlayer() != &p) return fail("It is not this player's turn");
    if (std::none_of(p.hand.begin(), p.hand.end(), [&](const auto& x){return x.get() == &c;}))
        return fail("Card is not in this player's hand");
    if (!c.canBePlayedBy(p)) return fail("Wrong card for role");
    if (!p.canPay(c.getManaCost())) return fail("Not enough mana");
    if (p.cardsPlayedThisTurn >= 3) return fail("Three-card turn limit reached");
    const auto* hc = dynamic_cast<const HeroCard*>(&c);
    if (!hc) return fail("DM effects are not implemented yet");
    if (hc->getEffect() == HeroEffect::BUFF) return fail("Buff rules are not implemented yet");
    if (hc->getEffect() == HeroEffect::HEAL) {
        for (const auto* h : heroes) if (h == &t && h->isAlive()) return {true, std::nullopt};
        return fail("Healing requires a living allied hero");
    }
    for (const auto& m : monsters) if (m.get() == &t && m->isAlive()) return {true, std::nullopt};
    return fail("Attack requires a living monster in this match");
}

void Match::apply(const Card& c, Targetable& t) {
    auto* p = getActivePlayer();
    if (!p) throw std::logic_error("No active player");
    auto check = validate(*p, c, t);
    if (!check.valid) throw std::invalid_argument(check.reason.value());
    p->playCard(c, t); checkWinner();
}

void Match::checkWinner() {
    if (std::none_of(heroes.begin(), heroes.end(), [](const auto* h){ return h->isAlive(); })) {
        winner = Side::DUNGEON_MASTER; return;
    }
    bool hadBoss = false, livingBoss = false, livingMonster = false;
    for (const auto& m : monsters) {
        hadBoss = hadBoss || m->isBoss();
        livingBoss = livingBoss || (m->isBoss() && m->isAlive());
        livingMonster = livingMonster || m->isAlive();
    }
    if ((hadBoss && !livingBoss) || !livingMonster) winner = Side::HEROES;
}

void Match::endTurn(Player& p) {
    if (getActivePlayer() != &p) throw std::invalid_argument("Not the active player");
    if (phase == Phase::HERO_TURN) {
        do { ++activeHero; } while (activeHero < heroes.size() && !heroes[activeHero]->isAlive());
        if (activeHero < heroes.size()) beginTurn(*heroes[activeHero]);
        else { phase = Phase::DM_TURN; beginTurn(*dm); }
    } else {
        phase = Phase::MONSTER_ACTIONS;

        // Perform the attacks in the order the DM selected them.
        for (std::size_t i = 0; i < monsterActions.size(); i++) {
            MonsterAction& action = monsterActions[i];

            // Skip attacks involving a defeated monster or hero.
            if (!action.monster->isAlive() || !action.target->isAlive()) {
                continue;
            }

            action.monster->attack(*action.card, *action.target);

            // Stop immediately if the attack ends the match.
            checkWinner();

            if (isFinished()) {
                break;
            }
        }

        // These orders must not run again next round.
        monsterActions.clear();

        checkWinner();
        if (isFinished()) return;
        if (round >= maxRounds) { winner = Side::DUNGEON_MASTER; return; }
        phase = Phase::ROUND_START; ++round; activeHero = 0;
        while (!heroes[activeHero]->isAlive()) ++activeHero;
        phase = Phase::HERO_TURN; beginTurn(*heroes[activeHero]);
    }
}

bool Match::isFinished() const {
    return winner.has_value();
}

int Match::getRound() const {
    return round;
}

Phase Match::getPhase() const {
    return phase;
}

const std::string& Match::getMatchId() const {
    return matchId;
}

std::optional<Side> Match::getWinner() const {
    return winner;
}

void Match::queueMonsterAttack(DungeonMaster& player, Monster& monster, const Card& card, Hero& target) {
    // Only the active DM can give monster orders.
    if (!started || isFinished()) {
        throw std::logic_error("Match is not active");
    }

    if (phase != Phase::DM_TURN || getActivePlayer() != &player) {
        throw std::invalid_argument("It is not this DM's turn");
    }

    // The card must be in the DM's hand.
    bool cardInHand = false;

    for (std::size_t i = 0; i < player.hand.size(); i++) {
        if (player.hand[i].get() == &card) {
            cardInHand = true;
            break;
        }
    }

    if (!cardInHand) {
        throw std::invalid_argument("Card is not in the DM's hand");
    }

    // Only DM attack cards can create an attack order.
    const DMCard* dmCard = dynamic_cast<const DMCard*>(&card);

    if (dmCard == nullptr) {
        throw std::invalid_argument("A DM card is required");
    }

    if (dmCard->getEffect() != DMEffect::ATTACK) {
        throw std::invalid_argument("An attack card is required");
    }

    if (!card.canBePlayedBy(player)) {
        throw std::invalid_argument("Wrong card for role");
    }

    if (!player.canPay(card.getManaCost())) {
        throw std::invalid_argument("Not enough mana");
    }

    if (player.cardsPlayedThisTurn >= 3) {
        throw std::invalid_argument("Three-card turn limit reached");
    }

    // The selected monster must belong to this match.
    bool monsterInMatch = false;

    for (int i = 0; i < monsters.size(); i++) {
        if (monsters[i].get() == &monster) {
            monsterInMatch = true;
            break;
        }
    }

    if (!monsterInMatch || !monster.isAlive()) {
        throw std::invalid_argument("Select a living monster in this match");
    }

    // The target must be a living hero in this match.
    bool heroInMatch = false;

    for (int i = 0; i < heroes.size(); i++) {
        if (heroes[i] == &target) {
            heroInMatch = true;
            break;
        }
    }

    if (!heroInMatch || !target.isAlive()) {
        throw std::invalid_argument("Select a living hero in this match");
    }

    // Store the damage without dealing damage yet.
    MonsterAction action;
    action.monster = &monster;
    action.card = &card;
    action.target = &target;

    monsterActions.push_back(action);

    // Pay for the card 
    player.mana -= card.getManaCost();
    player.cardsPlayedThisTurn++;
    player.discard(card);
}