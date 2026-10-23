#include "../include/Renderer.h"
#include "imgui.h"

void drawCharacter(const Character& c)
{
    ImGui::BeginChild(c.name.c_str(), ImVec2(170, 220), true);

    ImGui::Text("%s", c.name.c_str());

    char text[32];
    snprintf(text, sizeof(text), "%d / %d", c.health, c.maxHealth);
    ImGui::ProgressBar((float)c.health / c.maxHealth, ImVec2(150, 0), text);

    // if is used to only draw the mana bar for characters with mana
    if (c.maxMana > 0){

        char manaText[32];
        snprintf(manaText, sizeof(manaText), "%d / %d", c.mana, c.maxMana);
        ImGui::ProgressBar((float)c.mana / c.maxMana, ImVec2(150, 0), manaText);
    }

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 150, p.y + 80), IM_COL32(90, 90, 90, 255));
    ImGui::Dummy(ImVec2(150, 80)); //needs to be changed once we agree on pic size
        
    ImGui::EndChild();

}
