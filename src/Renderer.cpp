#include "Renderer.h"
#include "imgui.h"
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

void drawCharacter(const Character& c)
{
    ImGui::BeginChild(c.name.c_str(), ImVec2(240, 360), false);

    ImGui::Text("%s", c.name.c_str());

    char text[32];
    snprintf(text, sizeof(text), "%d / %d", c.health, c.maxHealth);
    ImGui::ProgressBar((float)c.health / c.maxHealth, ImVec2(220, 0), text);

    if (c.maxMana > 0) {
        char manaText[32];
        snprintf(manaText, sizeof(manaText), "%d / %d", c.mana, c.maxMana);
        ImGui::ProgressBar((float)c.mana / c.maxMana, ImVec2(220, 0), manaText);
    }

    ImGui::Spacing();

    if (c.textureID != 0)
    {
        ImGui::Image((ImTextureID)(intptr_t)c.textureID, ImVec2(220, 260));
    }
    else
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 220, p.y + 260), IM_COL32(90, 90, 90, 255));
        ImGui::Dummy(ImVec2(220, 260));
    }

    ImGui::EndChild();
}
void drawCard (const CardView& c, bool playable)
{
    ImGui::BeginDisabled(!playable);
    ImGui::BeginChild(c.name.c_str(), ImVec2(130, 110), true);

    ImGui::Text("%s", c.name.c_str());
    ImGui::Text("Cost: %d", c.manaCost);

    switch (c.effect)
    {
        case HeroEffect::ATTACK: ImGui::Text("Deal %d damage", c.value); break;
        case HeroEffect::HEAL:   ImGui::Text("+%d HP", c.value);        break;
        default:                 ImGui::Text("Effect %d", c.value);      break;
    }

    ImGui::EndChild();
    ImGui::EndDisabled();
}