#ifndef DUNGEON_PARTY_RENDERER_H
#define DUNGEON_PARTY_RENDERER_H

#include <string>
#include <glad/glad.h>
#include "Types.h"

struct Character {
    std::string name;
    int health;
    int maxHealth;
    int mana;
    int maxMana;
    GLuint textureID = 0;
};

// decoupled from card.h, graphics stay independent of the game engine rules.
struct CardView {
    std::string name;
    int manaCost;
    HeroEffect effect;
    int value;
};

enum class Screen { Start, Battle };

bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
void drawCharacter(const Character& c);
void drawCard(const CardView& c, bool playable);

class Renderer {
};

#endif // DUNGEON_PARTY_RENDERER_H