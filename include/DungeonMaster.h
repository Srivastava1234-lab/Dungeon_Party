#ifndef DUNGEON_PARTY_DUNGEONMASTER_H
#define DUNGEON_PARTY_DUNGEONMASTER_H
#include "Player.h"

class DungeonMaster : public Player {
public:
    using Player::Player;
    bool acceptsCard(const Card& card) const override;
    void setTraps(const Card& card); // will be implemented later when doing traps
};


#endif