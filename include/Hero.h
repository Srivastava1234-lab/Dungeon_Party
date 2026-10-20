#ifndef DUNGEON_PARTY_HERO_H
#define DUNGEON_PARTY_HERO_H


class Hero {
private:
    int health;

public:
    explicit Hero(int startingHealth);
    int getHealth() const;
    void takeDamage(int amount);
    bool isAlive() const;
};


#endif //DUNGEON_PARTY_HERO_H
