#ifndef DUNGEON_PARTY_TURN_INDICATOR_H
#define DUNGEON_PARTY_TURN_INDICATOR_H
#include "Match.h"
#include <string>

inline std::string turnIndicatorText(const Match& match) {
    const auto prefix = "Round " + std::to_string(match.getRound()) + " | ";
    if (match.isFinished())
        return prefix + (match.getWinner() == Side::HEROES ? "Heroes win!" : "Dungeon Master wins!");
    if (!match.getActivePlayer()) return "Waiting for match to start";
    switch (match.getPhase()) {
    case Phase::HERO_TURN: return prefix + "Heroes' turn";
    case Phase::DM_TURN: return prefix + "Dungeon Master's turn";
    case Phase::MONSTER_ACTIONS: return prefix + "Monster actions";
    default: return prefix + "Round starting";
    }
}

void drawTurnIndicator(const Match& match);
#endif