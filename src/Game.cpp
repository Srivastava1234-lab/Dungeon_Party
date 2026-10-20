#include "../include/Game.h"

Game::Game()
    : hero(30), 
    boss(20),
    round(1), 
    phase(Phase::HERO_TURN){
    }

void Game::endTurn(){
    if(phase == Phase::HERO_TURN){
        phase = Phase::MONSTER_TURN;
    } else {
        phase = Phase::HERO_TURN;
        round++;
    }
}

int Game::getRound() const{
    return round;
}
Phase Game::getPhase() const{
    return phase;
}

const Hero& Game::getHero() const{
    return hero;
}

const Boss& Game::getBoss() const {
    return boss;
}

