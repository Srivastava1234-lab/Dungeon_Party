#ifndef DUNGEON_PARTY_PLAYER_H
#define DUNGEON_PARTY_PLAYER_H
#include <memory>
#include <vector>
#include "Card.h"


class Player{
    friend class Match;

protected:
    std::string name;
    int mana;
    int maxMana;
    bool bot;
    int cardsPlayedThisTurn = 0;
    std::string playerId;
    std::vector<std::unique_ptr<Card>> deck, hand, discardPile;
public:
    Player(std::string id, std::string name, int maximumMana, bool bot = false);
    virtual ~Player() = default;
    virtual bool acceptsCard(const Card& card) const = 0;
    void addToDeck(std::unique_ptr<Card> card);
    const Card* drawCard(); // nullptr means empty deck; no slicing of derived cards
    void refillMana();
    bool canPay(int cost) const;
    const std::string& getName() const;
    const std::string& getPlayerId() const;
    bool isBot() const;
    int getMana() const;
    int getMaxMana() const;
    int getCardsPlayedThisTurn() const;
    const Card& getHandCard(std::size_t index) const;
    std::size_t getHandSize() const;
    std::size_t getDiscardSize() const;
private:
    // Match alone authorizes actions; callers cannot bypass turn validation.
    void playCard(const Card& card, Targetable& target);
    void discard(const Card& card);
};

#endif