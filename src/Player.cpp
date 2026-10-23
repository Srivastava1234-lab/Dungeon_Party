#include "Player.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
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
        return nullptr;
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
void Player::playCard(const Card& c, Targetable& t) {
    // Match has validated this action. Failed effects must not charge mana.
    c.applyEffect(t);
    mana -= c.getManaCost();
    ++cardsPlayedThisTurn;
    discard(c);
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
