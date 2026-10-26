#include "Match.h"
#include "HeroCard.h"

#include <stdexcept>
#include <utility>

Match::Match(std::string id, int limit)
    : matchId(std::move(id)), maxRounds(limit) {

    if (maxRounds <= 0 || matchId.empty()) {
        throw std::invalid_argument("Invalid match setup");
    }
}

void Match::addPlayer(std::unique_ptr<Player> player) {
    if (started || !player) {
        throw std::logic_error("Cannot add player");
    }

    // Check if another player already has this ID.
    for (std::size_t i = 0; i < players.size(); i++) {
        if (players[i]->getPlayerId() == player->getPlayerId()) {
            throw std::invalid_argument("Duplicate player ID");
        }
    }

    Hero* hero = dynamic_cast<Hero*>(player.get());
    DungeonMaster* master = dynamic_cast<DungeonMaster*>(player.get());

    if (hero == nullptr && master == nullptr) {
        throw std::invalid_argument("Unsupported player role");
    }

    players.push_back(std::move(player));
}

void Match::addMonster(std::unique_ptr<Monster> monster) {
    if (started || !monster) {
        throw std::logic_error("Configure monsters before start");
    }

    monsters.push_back(std::move(monster));
}

void Match::start() {
    if (started) {
        throw std::logic_error("Match already started");
    }

    heroes.clear();
    dm = nullptr;

    // Separate the hero players from the DM player.
    for (int i = 0; i < players.size(); i++) {
        Hero* hero = dynamic_cast<Hero*>(players[i].get());

        if (hero != nullptr) {
            heroes.push_back(hero);
        }

        DungeonMaster* master = dynamic_cast<DungeonMaster*>(players[i].get());

        if (master != nullptr) {
            if (dm != nullptr) {
                throw std::logic_error("Exactly one DM required");
            }

            dm = master;
        }
    }

    if (heroes.empty() || heroes.size() > 6 || dm == nullptr || monsters.empty()) {

        throw std::logic_error(
            "Need 1-6 heroes, exactly one DM and at least one monster"
        );
    }

    started = true;
    round = 1;
    activeHero = 0;
    phase = Phase::HERO_TURN;

    beginTurn(*heroes[activeHero]);
}

void Match::beginTurn(Player& player) {
    player.refillMana();
    player.cardsPlayedThisTurn = 0;
    player.drawCard();
}

Player* Match::getActivePlayer() const {
    if (!started || isFinished()) {
        return nullptr;
    }

    if (phase == Phase::HERO_TURN) {
        return heroes.at(activeHero);
    }

    if (phase == Phase::DM_TURN) {
        return dm;
    }

    return nullptr;
}

CheckResult Match::validate(const Player& player, const Card& card, const Targetable& target) const {

    if (!started || isFinished()) {
        return {false, "Match is not active"};
    }

    if (getActivePlayer() != &player) {
        return {false, "It is not this player's turn"};
    }

    // Check if the card belongs to the player's hand.
    bool cardInHand = false;

    for (int i = 0; i < player.hand.size(); i++) {
        if (player.hand[i].get() == &card) {
            cardInHand = true;
            break;
        }
    }

    if (!cardInHand) {
        return {false, "Card is not in this player's hand"};
    }

    if (!card.canBePlayedBy(player)) {
        return {false, "Wrong card for role"};
    }

    if (!player.canPay(card.getManaCost())) {
        return {false, "Not enough mana"};
    }

    if (player.cardsPlayedThisTurn >= 3) {
        return {false, "Three-card turn limit reached"};
    }

    const HeroCard* heroCard = dynamic_cast<const HeroCard*>(&card);

    if (heroCard == nullptr) {
        return {false, "DM effects are not implemented yet"};
    }

    if (heroCard->getEffect() == HeroEffect::BUFF) {
        return {false, "Buff rules are not implemented yet"};
    }

    // Healing targets a living hero in this match.
    if (heroCard->getEffect() == HeroEffect::HEAL) {
        for (int i = 0; i < heroes.size(); i++) {
            if (heroes[i] == &target && heroes[i]->isAlive()) {
                return {true, std::nullopt};
            }
        }

        return {false, "Healing requires a living allied hero"};
    }

    // Attacks targets a living monster in this match.
    for (int i = 0; i < monsters.size(); i++) {
        if (monsters[i].get() == &target &&monsters[i]->isAlive()) {
            return {true, std::nullopt};
        }
    }

    return {false, "Attack requires a living monster in this match"};
}

void Match::apply(const Card& card, Targetable& target) {
    Player* player = getActivePlayer();

    if (player == nullptr) {
        throw std::logic_error("No active player");
    }

    CheckResult result = validate(*player, card, target);

    if (!result.valid) {
        throw std::invalid_argument(result.reason.value());
    }

    player->playCard(card, target);
    checkWinner();
}

void Match::checkWinner() {
    bool livingHero = false;

    for (int i = 0; i < heroes.size(); i++) {
        if (heroes[i]->isAlive()) {
            livingHero = true;
            break;
        }
    }

    if (!livingHero) {
        winner = Side::DUNGEON_MASTER;
        return;
    }

    bool hadBoss = false;
    bool livingBoss = false;
    bool livingMonster = false;

    for (int i = 0; i < monsters.size(); i++) {
        if (monsters[i]->isBoss()) {
            hadBoss = true;

            if (monsters[i]->isAlive()) {
                livingBoss = true;
            }
        }

        if (monsters[i]->isAlive()) {
            livingMonster = true;
        }
    }

    if ((hadBoss && !livingBoss) || !livingMonster) {
        winner = Side::HEROES;
    }
}

void Match::endTurn(Player& player) {
    if (getActivePlayer() != &player) {
        throw std::invalid_argument("Not the active player");
    }

    if (phase == Phase::HERO_TURN) {
        activeHero++;

        while (activeHero < heroes.size()) {
            if (heroes[activeHero]->isAlive()) {
                break;
            }

            activeHero++;
        }

        if (activeHero < heroes.size()) {
            beginTurn(*heroes[activeHero]);
        } else {
            phase = Phase::DM_TURN;
            beginTurn(*dm);
        }
    } else {
        phase = Phase::MONSTER_ACTIONS;

        // Monster actions will be implemented later.
        checkWinner();

        if (isFinished()) {
            return;
        }

        if (round >= maxRounds) {
            winner = Side::DUNGEON_MASTER;
            return;
        }

        phase = Phase::ROUND_START;
        round++;
        activeHero = 0;

        // Find the first living hero for the new round.
        while (!heroes[activeHero]->isAlive()) {
            activeHero++;
        }

        phase = Phase::HERO_TURN;
        beginTurn(*heroes[activeHero]);
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
