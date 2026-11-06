#include "Player.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <random>
#include "Hero.h"
#include "HeroCard.h"

Player::Player(std::string id, std::string n, int maximum, bool b)
    : name(std::move(n)), 
    mana(maximum), 
    maxMana(maximum), 
    bot(b), 
    playerId(std::move(id)) {
    if (maximum < 0 || playerId.empty()){
         throw std::invalid_argument("Invalid player setup");
    }
}

void Player::addToDeck(std::unique_ptr<Card> c) {
    if (!c || !acceptsCard(*c)){
        throw std::invalid_argument("Card does not belong in this role's deck");
    }

    deck.push_back(std::move(c));
}

const Card* Player::drawCard() {
    if (deck.empty()){
        if(discardPile.empty()){
            return nullptr;     
        }

        //swap() moves the discard pile into the empty deck. shuffle() randomizes its order. If both piles are empty, drawing stops.
        deck.swap(discardPile);
        static std::mt19937 generator(std::random_device{}());
        std::shuffle(deck.begin(), deck.end(), generator);
    }    

    hand.push_back(std::move(deck.front()));
    deck.erase(deck.begin());
    return hand.back().get();
}

void Player::refillMana() { 
    mana = maxMana; 
}

bool Player::canPay(int cost) const { 
    return cost >= 0 && mana >= cost; 
}

void Player::discard(const Card& c) {
    auto it = std::find_if(hand.begin(), hand.end(), [&](const auto& x){ return x.get() == &c; });
    if (it == hand.end()){
        throw std::invalid_argument("Card is not in hand");
    }
    
    discardPile.push_back(std::move(*it));
    hand.erase(it);
}

void Player::playCard(const Card& card, const std::vector<Targetable*>& targets) {
    Hero* hero = dynamic_cast<Hero*>(this);
    const HeroCard* heroCard = dynamic_cast<const HeroCard*>(&card);

    bool damagingCard = false;

    if (heroCard != nullptr) {
        if (heroCard->getEffect() == HeroEffect::ATTACK || heroCard->getEffect() == HeroEffect::SPELL || heroCard->getEffect() == HeroEffect::FIREBALL) {
            damagingCard = true;
        }
    }

    if (hero != nullptr && damagingCard) {
        int damage = heroCard->getValue() - hero->getAttackReduction();

        if (damage < 0) {
            damage = 0;
        }

        // Apply reduced damage without changing the original card.
        HeroCard adjustedCard(card.getName(), card.getManaCost(), heroCard->getEffect(), damage,
                              heroCard->getTarget(), heroCard->getFreezeRounds());

        for (auto* target : targets) adjustedCard.applyEffect(*target);

        // Remove Weakness after the attack succeeds.
        hero->consumeAttackReduction();
    } else {
        // Healing and other effects work normally.
        for (auto* target : targets) card.applyEffect(*target);
    }

    mana -= card.getManaCost();
    cardsPlayedThisTurn++;
    discard(card);
}

const std::string& Player::getName() const {
    return name;
}

const std::string& Player::getPlayerId() const {
    return playerId;
}

bool Player::isBot() const {
    return bot;
}

int Player::getMana() const {
    return mana;
}

int Player::getMaxMana() const {
    return maxMana;
}

int Player::getCardsPlayedThisTurn() const {
    return cardsPlayedThisTurn;
}

const Card& Player::getHandCard(std::size_t index) const {
    return *hand.at(index);
}

std::size_t Player::getHandSize() const {
    return hand.size();
}

std::size_t Player::getDiscardSize() const {
    return discardPile.size();
}
