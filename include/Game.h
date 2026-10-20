#ifndef DUNGEON_PARTY_GAME_H
#define DUNGEON_PARTY_GAME_H
#include "Hero.h"
#include "Boss.h"


enum class Phase{
    HERO_TURN, 
    MONSTER_TURN 
};

class Game {
private:
    int round;
    Phase phase;
    Hero hero;
    Boss boss;

public:
    Game();
    void endTurn();
    int getRound() const;
    Phase getPhase() const;
    const Hero& getHero() const;
    const Boss& getBoss() const;
    
};


#endif //DUNGEON_PARTY_GAME_H
