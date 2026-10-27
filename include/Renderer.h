#ifndef DUNGEON_PARTY_RENDERER_H
#define DUNGEON_PARTY_RENDERER_H

#include <string>
#include <glad/glad.h>
#include "Types.h"

class Match;

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
bool drawCharacter(const Character& c);
bool drawCard(const CardView& c, bool playable, bool selected);
void drawTurnIndicator(const Match& match);
class Renderer {
};

#endif // DUNGEON_PARTY_RENDERER_H