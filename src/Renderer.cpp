#include "Renderer.h"
#include "Match.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
{
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
    {
        std::cerr << "Failed to load image '" << filename << "'. Reason: " << stbi_failure_reason() << "\n";
        return false;
    }

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;

    return true;
}

bool drawCharacter(const Character& c, float scale, bool highlighted)
{
    const float width = 240.0f * scale, barWidth = 220.0f * scale;
    // "character" is the ID inside the caller's PushID scope, so two "Goblin"s don't merge into one widget.
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 220, 90, 255));
    ImGui::BeginChild("character", ImVec2(width, 100.0f + 260.0f * scale), highlighted, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextUnformatted(c.name.c_str());

    char text[32];
    snprintf(text, sizeof(text), "%d / %d", c.health, c.maxHealth);
    ImGui::ProgressBar(c.maxHealth > 0 ? (float)c.health / c.maxHealth : 0.0f, ImVec2(barWidth, 0), text);

    if (c.maxMana > 0) {
        char manaText[32];
        snprintf(manaText, sizeof(manaText), "%d / %d", c.mana, c.maxMana);
        ImGui::ProgressBar((float)c.mana / c.maxMana, ImVec2(barWidth, 0), manaText);
    }

    ImGui::Spacing();

    if (c.textureID != 0)
    {
        ImGui::Image((ImTextureID)(intptr_t)c.textureID, ImVec2(barWidth, 260 * scale));
    }
    else
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + barWidth, p.y + 260 * scale), IM_COL32(90, 90, 90, 255));
        ImGui::Dummy(ImVec2(barWidth, 260 * scale));
    }

    ImGui::EndChild();
    if (highlighted) ImGui::PopStyleColor();
    return ImGui::IsItemClicked();
}

bool drawCard (const CardView& c, bool playable, bool selected)
{
    ImGui::BeginDisabled(!playable);

    if (selected)
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 220, 90, 255));

    // "card" is the ID inside the caller's PushID scope, so two cards with the same name stay separate.
    const float height = c.textureID ? 175.0f : 110.0f;
    ImGui::BeginChild("card", ImVec2(130, height), true, ImGuiWindowFlags_NoScrollbar);

    if (c.textureID)
        ImGui::Image((ImTextureID)(intptr_t)c.textureID, ImVec2(112, 70));

    ImGui::TextWrapped("%s", c.name.c_str());
    ImGui::Text("Cost: %d", c.manaCost);

    if (!c.effectText.empty()) {
        ImGui::TextWrapped("%s", c.effectText.c_str());
    } else {
        switch (c.effect)
        {
            case HeroEffect::ATTACK:   ImGui::Text("Deal %d damage", c.value);    break;
            case HeroEffect::SPELL:    ImGui::Text("Spell: %d damage", c.value);  break;
            case HeroEffect::FIREBALL: ImGui::Text("Fireball: %d", c.value);      break;
            case HeroEffect::HEAL:     ImGui::Text("+%d HP", c.value);            break;
            case HeroEffect::SHIELD:   ImGui::Text("Shield %d", c.value);         break;
            default:                   ImGui::Text("Effect %d", c.value);         break;
        }
    }

    ImGui::EndChild();

    bool clicked = ImGui::IsItemClicked();
    if (selected)
        ImGui::PopStyleColor();

    ImGui::EndDisabled();
    return clicked;
}

void drawTurnIndicator(const Match& match)
{
    ImGui::Text("Active Turn / Round: %d", match.getRound());
}