#ifndef DUNGEON_PARTY_RENDERER_H
#define DUNGEON_PARTY_RENDERER_H
#include <string>
using namespace std;

// datatype for all characters
struct Character {
    string name;
    int health;
    int maxHealth;
    int mana;
    int maxMana;
};
// to switch between screens
enum class Screen{ Start, Battle };

void drawCharacter(const Character& c);


class Renderer {
};


#endif //DUNGEON_PARTY_RENDERER_H
