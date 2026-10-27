#include "TurnIndicator.h"
#include "imgui.h"

void drawTurnIndicator(const Match& match) {
    const auto text = turnIndicatorText(match);
    ImGui::TextUnformatted(text.c_str());
    ImGui::Separator();
}