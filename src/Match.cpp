#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <iostream>

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

void Match::configureBoss(std::unique_ptr<Monster> boss, int arrivalRound, int attackPower) {
    if (started || pendingBoss || swarms.hasPendingBoss() || !boss || !boss->isBoss() ||
        arrivalRound < 0 || arrivalRound > maxRounds || attackPower < 0) {
        throw std::invalid_argument("Invalid final boss configuration");
    }
    bossAttack.emplace("Boss attack", 0, DMEffect::ATTACK, attackPower);
    if (arrivalRound == 0) {
        swarms.setBoss(std::move(boss)); // enters after the last swarm
    } else {
        pendingBoss = std::move(boss);
        bossArrivalRound = arrivalRound;
    }
}

void Match::addSwarm(std::string name, std::vector<std::unique_ptr<Monster>> swarmMonsters) {
    if (started) throw std::logic_error("Configure swarms before start");
    swarms.addSwarm(std::move(name), std::move(swarmMonsters));
}

bool Match::releaseNextGroup() {
    if (swarms.hasNextSwarm()) {
        SwarmManager::Swarm swarm = swarms.takeNextSwarm();
        for (auto& monster : swarm.monsters) monsters.push_back(std::move(monster));
        addPublicLog("Swarm " + std::to_string(swarms.swarmsReleased()) + " of " +
                     std::to_string(swarms.swarmCount()) + " appears: " + swarm.name + ".");
        return true;
    }
    if (swarms.hasPendingBoss()) {
        std::unique_ptr<Monster> boss = swarms.takeBoss();
        finalBoss = boss.get();
        addPublicLog("All swarms are defeated. The Dungeon Master enters the battle as " + boss->getName() + "!");
        monsters.push_back(std::move(boss));
        return true;
    }
    return false;
}

void Match::spawnBoss() {
    if (pendingBoss && round == bossArrivalRound) {
        finalBoss = pendingBoss.get();
        monsters.push_back(std::move(pendingBoss));
    }
}

void Match::start() {
    if (started){
        throw std::logic_error("Match already started");
    } 
    heroes.clear(); dm = nullptr;
    for (const auto& p : players) {
        if (auto* h = dynamic_cast<Hero*>(p.get())){
            heroes.push_back(h);
        } 
        if (auto* d = dynamic_cast<DungeonMaster*>(p.get())) {
            if (dm){
                throw std::logic_error("Exactly one DM required");
            } 
            dm = d;
        }
    }
    if (monsters.empty()) releaseNextGroup(); // levels with swarms start with the first swarm
    if (heroes.empty() || heroes.size() > 6 || !dm || monsters.empty()) {
        throw std::logic_error("Need 1-6 heroes, exactly one DM and at least one monster");
    }
    
    for (std::size_t i = 0; i < heroes.size(); i++) {
        for (int cardNumber = 0; cardNumber < 5; cardNumber++) {
            heroes[i]->drawCard();
        }
    }

    started = true;
    round = 1;
    spawnBoss();
    activeHero = 0;
    phase = Phase::HERO_TURN;

    addPublicLog("The match started.");
    beginTurn(*heroes.front());
}

void Match::beginTurn(Player& player) {
    addPublicLog(player.getName() + "'s turn started.");
    player.refillMana();
    player.cardsPlayedThisTurn = 0;

    Hero* hero = dynamic_cast<Hero*>(&player);

    if (hero != nullptr) {
        for (int i = 0; i < 2; i++) {
            if (player.getHandSize() >= 7) {
                break; 
           }

            if (player.drawCard() == nullptr) {
                break;
            }
        }
        
    } else {
        player.drawCard();
    }    
}

Player* Match::getActivePlayer() const {
    if (!started || isFinished()) return nullptr;
    if (phase == Phase::HERO_TURN) return heroes.at(activeHero);
    if (phase == Phase::DM_TURN) return dm;
    return nullptr;
}

CheckResult Match::validate(const Player& p, const Card& c, const Targetable& t) const {
    auto fail = [](const char* s){ 
        return CheckResult{
            false, std::string(s)
        }; 
    };

    if (!started || isFinished()){ 
        return fail("Match is not active");
    }

    if (getActivePlayer() != &p){
         return fail("It is not this player's turn");
    }


    if (std::none_of(p.hand.begin(), p.hand.end(), [&](const auto& x){return x.get() == &c;})){
        return fail("Card is not in this player's hand");
    }
        
    if (!c.canBePlayedBy(p)) {
        return fail("Wrong card for role");
    }
    if (!p.canPay(c.getManaCost())){
        return fail("Not enough mana");
    } 

    if (p.cardsPlayedThisTurn >= 3){
        return fail("Three-card turn limit reached");
    } 

    const DMCard* dmCard = dynamic_cast<const DMCard*>(&c);

    if (dmCard != nullptr) {
        if (dmCard->getEffect() != DMEffect::WEAKNESS && dmCard->getEffect() != DMEffect::MANA_TRAP && dmCard->getEffect() != DMEffect::DISCARD_TRAP && dmCard->getEffect() != DMEffect::MANA_DRAIN_TRAP) {
            return fail("This DM card requires a living hero in this match");        
        }

        for (std::size_t i = 0; i < heroes.size(); i++) {
            if (heroes[i] == &t && heroes[i]->isAlive()) {
                return {true, std::nullopt};
            }
        }

        return fail("Weakness requires a living hero in this match");
    }

    const auto* hc = dynamic_cast<const HeroCard*>(&c);
    if (!hc){
        return fail("DM effects are not implemented yet");
    } 
    if (hc->getEffect() == HeroEffect::BUFF){
        return fail("Buff rules are not implemented yet");
    } 
    if (hc->getEffect() == HeroEffect::HEAL || hc->getEffect() == HeroEffect::SHIELD) {

        const Hero* caster = dynamic_cast<const Hero*>(&p);

        if (caster == nullptr) {
            return fail("Only heroes can play this card");
        }

        for (std::size_t i = 0; i < heroes.size(); i++) {
            if (heroes[i] == &t && heroes[i]->isAlive()) {
                bool targetingSelf = heroes[i] == caster;

                if (!targetingSelf) {
                    if (hc->getEffect() == HeroEffect::SHIELD) {
                        if (!caster->canShieldAllies()) {
                            return fail(
                                "This hero cannot shield allies"
                            );
                        }
                    } else {
                        if (!caster->canHealAllies()) {
                            return fail(
                                "This hero cannot heal allies"
                            );
                        }

                        if (hc->getSupportTarget() ==
                        SupportTarget::SELF_ONLY) {
                            return fail(
                                "This healing card can only target yourself"
                            );
                        }
                    }
                }

                return {true, std::nullopt};
            }
        }

        return fail("Select a living allied hero");
    }

    for (const auto& m : monsters){
        if (m.get() == &t && m->isAlive()) {
            if (hc->getEffect() == HeroEffect::SPELL &&
                hc->getTarget() == SpellTarget::BOSS_ONLY && !m->isBoss()) {
                return fail("This spell requires the boss");
            }
            return {true, std::nullopt};
        }
    } 
    return fail("Attack requires a living monster in this match");
}

void Match::apply(const Card& card, Targetable& target) {
    Player* player = getActivePlayer();

    if (player == nullptr) {
        throw std::logic_error("No active player");
    }

    CheckResult check = validate(*player, card, target);

    if (!check.valid) {
        throw std::invalid_argument(check.reason.value());
    }

    std::vector<Targetable*> targets;

    const HeroCard* heroCard = dynamic_cast<const HeroCard*>(&card);

    if (heroCard != nullptr && heroCard->getEffect() == HeroEffect::SPELL && heroCard->getTarget() == SpellTarget::ALL_ENEMIES) {

        for (std::size_t i = 0; i < monsters.size(); i++) {
            if (monsters[i]->isAlive()) {
                targets.push_back(monsters[i].get());
            }
        }
    } else {
        targets.push_back(&target);
    }

    // Prepare public messages, but don't publish them yet.
    std::vector<std::string> messages;

    if (heroCard != nullptr) {
        HeroEffect effect = heroCard->getEffect();

        for (std::size_t i = 0; i < targets.size(); i++) {
            Hero* targetHero = dynamic_cast<Hero*>(targets[i]);
            Monster* targetMonster = dynamic_cast<Monster*>(targets[i]);

            if (effect == HeroEffect::HEAL && targetHero != nullptr) {
                messages.push_back(player->getName() + " used healing on " + targetHero->getName() + ".");

            } else if (effect == HeroEffect::SHIELD && targetHero != nullptr) {
                messages.push_back(player->getName() + " shielded " + targetHero->getName() + ".");

            } else if (targetMonster != nullptr && (effect == HeroEffect::ATTACK || effect == HeroEffect::SPELL || effect == HeroEffect::FIREBALL)) {
                messages.push_back(player->getName() + " attacked " + targetMonster->getName() + ".");
            }
        }
    }

    player->playCard(card, targets);

    // Publish only after the play succeeds.
    for (std::size_t i = 0; i < messages.size(); i++) {
        addPublicLog(messages[i]);
    }

    for (std::size_t i = 0; i < player->triggeredTrapMessages.size(); i++) {
        addPublicLog(player->triggeredTrapMessages[i]);
    }

    player->triggeredTrapMessages.clear();

    checkWinner();
}

void Match::checkWinner() {
    // Avoid recording the result more than once.
    if (winner.has_value()) {
        return;
    }

    bool livingHero = false;

    for (std::size_t i = 0; i < heroes.size(); i++) {
        if (heroes[i]->isAlive()) {
            livingHero = true;
            break;
        }
    }

    if (!livingHero) {
        winner = Side::DUNGEON_MASTER;
        addPublicLog("The Dungeon Master wins.");
        return;
    }

    // SCRUM-44: the heroes win only after defeating EVERY monster, the boss included.
    // While the boss hasn't arrived yet, the match cannot be won.
    if (pendingBoss) {
        return;
    }

    bool livingMonster = false;

    for (std::size_t i = 0; i < monsters.size(); i++) {
        if (monsters[i]->isAlive()) {
            livingMonster = true;
            break;
        }
    }

    if (!livingMonster) {
        // The battlefield is clear: the next swarm or the Dungeon Master's boss enters.
        // The heroes only win when nothing is left to come.
        if (started && releaseNextGroup()) {
            return;
        }
        winner = Side::HEROES;
        addPublicLog("The heroes win.");
    }
}

void Match::endTurn(Player& p) {
    if (getActivePlayer() != &p){
        throw std::invalid_argument("Not the active player");
    } 
    if (phase == Phase::HERO_TURN) {
        // An untriggered Mana Trap expires after this hero's turn.
        Hero* endingHero = dynamic_cast<Hero*>(&p);

        if (endingHero != nullptr) {
            endingHero->clearManaTrap();
        }

        do { ++activeHero; } while (activeHero < heroes.size() && !heroes[activeHero]->isAlive());
        if (activeHero < heroes.size()){
            beginTurn(*heroes[activeHero]);
        } 
        else { 
            phase = Phase::DM_TURN; beginTurn(*dm); 
        }
    } else {
        phase = Phase::MONSTER_ACTIONS;

        // Apply Burn before monsters perform their queued attacks.
        for (std::size_t i = 0; i < monsters.size(); i++) {
            Monster* monster = monsters[i].get();
            int healthBefore = monster->getHealth();

            monster->processBurn();

            int healthLost = healthBefore - monster->getHealth();

            if (healthLost > 0) {
                addPublicLog(monster->getName() + " lost " + std::to_string(healthLost) + " HP from Burn.");

                if (!monster->isAlive()) {
                    addPublicLog(monster->getName() + " was defeated.");
                }
            }
        }

        checkWinner();

        if (isFinished()) {
            monsterActions.clear();
            return;
        }

        // Perform the attacks in the order the DM selected them.
        for (std::size_t i = 0; i < monsterActions.size(); i++) {
            MonsterAction& action = monsterActions[i];

            // Skip attacks involving a defeated monster or hero.
            if (!action.monster->isAlive() || !action.target->isAlive()) {
                continue;
            }

            if (action.monster->skipFrozenAttack()) continue;

            int healthBefore = action.target->getHealth();
            int shieldBefore = action.target->getShield();

            action.monster->attack(*action.card, *action.target);

            int healthLost = healthBefore - action.target->getHealth();
            int shieldLost = shieldBefore - action.target->getShield();

            addPublicLog(action.monster->getName() + " attacked " + action.target->getName() + ": " + std::to_string(healthLost) + " HP lost, " + std::to_string(shieldLost) + " shield absorbed.");

            if (!action.target->isAlive()) {
                addPublicLog(
                action.target->getName() + " was defeated."
                );
            }  
            // Stop immediately if the attack ends the match.
            checkWinner();

            if (isFinished()) {
                break;
            }
        }

        // These orders must not run again next round.
        monsterActions.clear();

        // SCRUM-55: the round is over, every timed effect loses one round.
        for (std::size_t i = 0; i < heroes.size(); i++) {
            heroes[i]->tickEffects();
        }

        checkWinner();
        if (isFinished()){
             return;
        }
        if (round >= maxRounds) { 
            winner = Side::DUNGEON_MASTER;
            addPublicLog("The round limit was reached. The Dungeon Master wins.");

            return;
        }

        phase = Phase::ROUND_START; ++round; activeHero = 0;
        spawnBoss();
        while (!heroes[activeHero]->isAlive()){
             ++activeHero;
        } 
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

bool Match::canQueueBossAttack() const {
    return phase == Phase::DM_TURN && getActivePlayer() == dm && finalBoss &&
        finalBoss->isAlive() &&
        std::none_of(monsterActions.begin(), monsterActions.end(),
                     [this](const MonsterAction& action) { return action.monster == finalBoss; });
}

void Match::queueBossAttack(DungeonMaster& player, Hero& target) {
    if (getActivePlayer() != &player || !canQueueBossAttack()) {
        throw std::invalid_argument("The boss cannot queue an attack now");
    }
    if (std::find(heroes.begin(), heroes.end(), &target) == heroes.end() || !target.isAlive()) {
        throw std::invalid_argument("Select a living hero in this match");
    }
    monsterActions.push_back({finalBoss, &*bossAttack, &target});
}

void Match::queueMonsterAttack(DungeonMaster& player, Monster& monster, const Card& card, Hero& target) {
    if (&monster == finalBoss) {
        throw std::invalid_argument("Use the boss attack order");
    }
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

void Match::addPublicLog(const std::string& message) {
    if (message.empty()) {
        return;
    }

    actionLog.push_back(
        "Round " + std::to_string(round) + ": " + message
    );

    // Keep only the latest 100 messages.
    if (actionLog.size() > 100) {
        actionLog.erase(actionLog.begin());
    }
}

const std::vector<std::string>& Match::getActionLog() const {
    return actionLog;
}