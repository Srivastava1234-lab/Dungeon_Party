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


enum class LobbyRole { None, Warrior, Healer, Wizard, DM };
struct LobbyPlayer {
    std::string name;
    LobbyRole role;
};

// decoupled from card.h, graphics stay independent of the game engine rules.
struct CardView {
    std::string name;
    int manaCost;
    HeroEffect effect;
    int value;
    GLuint textureID = 0;     // card artwork (0 = text only)
    std::string effectText;   // overrides the generated effect line when not empty (e.g. DM cards)
};

enum class Screen { Start, Menu, Lobby, Battle};

bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
bool drawCharacter(const Character& c, float scale = 1.0f, bool highlighted = false);
bool drawCard(const CardView& c, bool playable, bool selected);
void drawTurnIndicator(const Match& match);
class Renderer {
};

#endif // DUNGEON_PARTY_RENDERER_H