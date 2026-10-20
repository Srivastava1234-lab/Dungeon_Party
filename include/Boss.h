#ifndef DUNGEON_PARTY_BOSS_H
#define DUNGEON_PARTY_BOSS_H


class Boss {
private:
    int health;

public:
    Boss(int startingHealth);

    int getHealth() const;

    void takeDamage(int amount);
    bool isAlive() const;
};


#endif //DUNGEON_PARTY_BOSS_H
