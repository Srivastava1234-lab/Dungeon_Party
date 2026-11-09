// dungeon_online - the networked game client (Sprint 2 prototype).
//
// Menu -> (Host room | Join with code) -> Lobby -> Battle
// The room code is shown in the top-left corner of the lobby and the battle screen.
// All game state comes from the server; this program only draws it and sends requests.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "GameData.h"
#include "NetClient.h"
#include "Protocol.h"
#include "Renderer.h"

using nlohmann::json;

namespace {

std::string projectPath(const std::string& relative) {
#ifdef DUNGEON_PARTY_PROJECT_DIR
    return (std::filesystem::path(DUNGEON_PARTY_PROJECT_DIR) / relative).string();
#else
    return relative;
#endif
}

enum class OnlineScreen { Menu, Connecting, Lobby, Battle };

HeroEffect effectFromName(const std::string& name) {
    if (name == "HEAL") return HeroEffect::HEAL;
    if (name == "BUFF") return HeroEffect::BUFF;
    if (name == "SPELL") return HeroEffect::SPELL;
    return HeroEffect::ATTACK;
}

// Safe readers: the state comes over the network, so a missing field must not crash the client.
int intOr(const json& j, const char* key, int fallback = 0) {
    auto it = j.find(key);
    return (it != j.end() && it->is_number_integer()) ? it->get<int>() : fallback;
}
std::string strOr(const json& j, const char* key, const std::string& fallback = "") {
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

// The room code badge in the top-left corner, with a button to copy it.
void drawRoomCode(const std::string& code, float fontScale) {
    ImGui::SetWindowFontScale(fontScale);
    ImGui::TextUnformatted(("Room " + code).c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(code.c_str());
}

void beginFullscreenWindow(const char* name) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
}

} // namespace

int main() {
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(1000, 650, "Dungeon Party - Online", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { glfwTerminate(); return -1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Textures: hero art by hero id, monster art by monster name (that's what the server sends).
    std::map<std::string, GLuint> heroTextures, monsterTextures;
    GLuint bgTexture = 0;
    int w = 0, h = 0;
    try {
        GameData data(GameData::defaultDataDirectory());
        for (const auto& hero : data.getHeroes()) {
            GLuint tex = 0;
            if (!hero.image.empty() && LoadTextureFromFile(projectPath(hero.image).c_str(), &tex, &w, &h)) heroTextures[hero.id] = tex;
        }
        for (const auto& monster : data.getMonsters()) {
            GLuint tex = 0;
            if (!monster.image.empty() && LoadTextureFromFile(projectPath(monster.image).c_str(), &tex, &w, &h)) monsterTextures[monster.name] = tex;
        }
    } catch (const std::exception& e) {
        std::cerr << "Artwork not loaded: " << e.what() << "\n"; // the game still works without pictures
    }
    LoadTextureFromFile(projectPath("assets/Level_1_Artwork.png").c_str(), &bgTexture, &w, &h);
    auto textureOr0 = [](const std::map<std::string, GLuint>& textures, const std::string& key) {
        auto it = textures.find(key);
        return it == textures.end() ? GLuint(0) : it->second;
    };

    // ---- Client state
    NetClient client;
    OnlineScreen screen = OnlineScreen::Menu;
    json pendingRequest;  // "create" or "join", sent as soon as the connection is up
    std::string notice;   // last error / info line shown to the player
    std::string roomCode, myId, hostId;
    json lobbyPlayers = json::array();
    json state;           // latest match state from the server
    int selectedCard = -1;

    char nameBuf[net::MAX_NAME_LENGTH + 1] = "Player";
    char serverBuf[128] = "127.0.0.1";
    int port = net::DEFAULT_PORT;
    char codeBuf[16] = "";

    auto backToMenu = [&](const std::string& why) {
        client.disconnect();
        screen = OnlineScreen::Menu;
        notice = why;
        roomCode.clear(); myId.clear(); hostId.clear();
        state = nullptr; selectedCard = -1;
    };

    auto startConnecting = [&](json request) {
        notice.clear();
        if (!client.connect(serverBuf, static_cast<std::uint16_t>(port))) { notice = client.lastError(); return; }
        pendingRequest = std::move(request);
        screen = OnlineScreen::Connecting;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ---- Network: once per frame, never blocks
        client.poll();
        if (screen != OnlineScreen::Menu && client.status() == NetClient::Status::Disconnected)
            backToMenu(client.lastError().empty() ? "Disconnected" : client.lastError());
        if (screen == OnlineScreen::Connecting && client.status() == NetClient::Status::Connected && !pendingRequest.is_null()) {
            client.send(pendingRequest);
            pendingRequest = nullptr;
        }
        for (const json& message : client.takeMessages()) {
            const std::string type = strOr(message, "t");
            if (type == net::msg::JOINED) {
                roomCode = strOr(message, "code");
                myId = strOr(message, "you");
                screen = OnlineScreen::Lobby;
            } else if (type == net::msg::LOBBY) {
                roomCode = strOr(message, "code", roomCode);
                hostId = strOr(message, "host");
                lobbyPlayers = message.value("players", json::array());
                screen = OnlineScreen::Lobby;
            } else if (type == net::msg::STATE) {
                state = message;
                hostId = strOr(message, "host", hostId);
                screen = OnlineScreen::Battle;
            } else if (type == net::msg::ERROR) {
                notice = strOr(message, "msg");
                if (screen == OnlineScreen::Connecting) backToMenu(notice); // e.g. wrong room code
            } else if (type == net::msg::CLOSED) {
                backToMenu(strOr(message, "msg", "The room was closed"));
            }
        }

        glClearColor(0.12f, 0.34f, 0.56f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (screen == OnlineScreen::Menu || screen == OnlineScreen::Connecting) {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(440, 0));
            ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::TextUnformatted("DUNGEON PARTY");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Separator();

            if (screen == OnlineScreen::Connecting) {
                ImGui::Text("Connecting to %s:%d ...", serverBuf, port);
                if (ImGui::Button("Cancel", ImVec2(-1, 0))) backToMenu("");
            } else {
                ImGui::InputText("Your name", nameBuf, sizeof(nameBuf));
                ImGui::InputText("Server", serverBuf, sizeof(serverBuf));
                ImGui::InputInt("Port", &port);
                ImGui::Spacing();
                if (ImGui::Button("Host a new room", ImVec2(-1, 40)))
                    startConnecting({{"t", net::msg::CREATE}, {"name", nameBuf}});
                ImGui::Spacing();
                ImGui::InputTextWithHint("##code", "Room code", codeBuf, sizeof(codeBuf), ImGuiInputTextFlags_CharsUppercase);
                ImGui::SameLine();
                if (ImGui::Button("Join", ImVec2(-1, 0)))
                    startConnecting({{"t", net::msg::JOIN}, {"name", nameBuf}, {"code", codeBuf}});
            }
            if (!notice.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
            ImGui::End();
        } else if (screen == OnlineScreen::Lobby) {
            beginFullscreenWindow("Lobby");
            drawRoomCode(roomCode, 2.0f); // top-left
            ImGui::Separator();
            ImGui::Text("Give this code to your friends. Players (%zu/%zu):", lobbyPlayers.size(), net::MAX_PLAYERS_PER_ROOM);
            for (const auto& p : lobbyPlayers) {
                const std::string id = strOr(p, "id");
                ImGui::BulletText("%s%s%s", strOr(p, "name").c_str(), id == hostId ? "  (host)" : "", id == myId ? "  <- you" : "");
            }
            ImGui::Spacing();
            if (myId == hostId) {
                if (ImGui::Button("Start match", ImVec2(200, 40))) client.send({{"t", net::msg::START}});
            } else {
                ImGui::TextUnformatted("Waiting for the host to start...");
            }
            ImGui::SameLine();
            if (ImGui::Button("Leave", ImVec2(100, 40))) backToMenu("");
            if (!notice.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
            ImGui::End();
        } else if (screen == OnlineScreen::Battle && state.is_object()) {
            beginFullscreenWindow("Battle");
            const bool myTurn = !state["active"].is_null() && strOr(state, "active") == myId;

            // ---- Top bar: room code top-left, then turn info
            ImGui::BeginChild("TopBar", ImVec2(0, 44), true);
            drawRoomCode(roomCode, 1.4f);
            ImGui::SameLine(220);
            if (!state["winner"].is_null()) {
                ImGui::Text("Round %d | %s win!", intOr(state, "round"),
                            strOr(state, "winner") == "HEROES" ? "Heroes" : "Dungeon Master");
            } else {
                std::string whose = "Dungeon Master";
                for (const auto& hero : state["heroes"])
                    if (strOr(hero, "id") == strOr(state, "active")) whose = strOr(hero, "name");
                ImGui::Text("Round %d | %s", intOr(state, "round"), myTurn ? "YOUR TURN" : (whose + "'s turn").c_str());
                if (myTurn) {
                    ImGui::SameLine();
                    if (ImGui::Button("End turn")) { client.send({{"t", net::msg::END}}); selectedCard = -1; }
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Leave")) backToMenu("");
            ImGui::EndChild();

            // ---- Battlefield
            ImGui::BeginChild("Battlefield", ImVec2(0, ImGui::GetWindowHeight() - 230), true);
            if (bgTexture) {
                ImGui::SetCursorPos(ImVec2(0, 0));
                ImGui::Image((ImTextureID)(intptr_t)bgTexture, ImGui::GetContentRegionAvail());
                ImGui::SetCursorPos(ImVec2(10, 10));
            }
            int drawn = 0;
            for (const auto& hero : state["heroes"]) {
                if (drawn++ > 0) ImGui::SameLine();
                std::string label = strOr(hero, "name");
                if (strOr(hero, "id") == myId) label += " (you)";
                if (!hero.value("connected", true)) label += " [left]";
                drawCharacter({label, intOr(hero, "hp"), intOr(hero, "maxHp", 1), intOr(hero, "mana"), intOr(hero, "maxMana"),
                               textureOr0(heroTextures, strOr(hero, "heroId"))});
            }
            ImGui::SameLine(0, 60);
            drawn = 0;
            const json& monsters = state["monsters"];
            for (std::size_t i = 0; i < monsters.size(); ++i) {
                if (!monsters[i].value("alive", false)) continue;
                if (drawn++ > 0) ImGui::SameLine();
                const bool clicked = drawCharacter({strOr(monsters[i], "name"), intOr(monsters[i], "hp"), intOr(monsters[i], "maxHp", 1),
                                                   0, 0, textureOr0(monsterTextures, strOr(monsters[i], "name"))});
                // Click a monster with a card selected = ask the server to play it. The server decides.
                if (clicked && myTurn && selectedCard >= 0) {
                    client.send({{"t", net::msg::PLAY}, {"card", selectedCard}, {"target", i}});
                    selectedCard = -1;
                }
            }
            ImGui::EndChild();

            // ---- My hand (the server only ever sends me my own cards)
            ImGui::BeginChild("Hand", ImVec2(0, 0), true);
            int myMana = 0;
            for (const auto& hero : state["heroes"]) if (strOr(hero, "id") == myId) myMana = intOr(hero, "mana");
            if (!notice.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
            const float rowY = ImGui::GetCursorPosY() + 15.0f;
            const json hand = state.value("hand", json::array());
            for (std::size_t i = 0; i < hand.size(); ++i) {
                CardView view{strOr(hand[i], "name"), intOr(hand[i], "cost"), effectFromName(strOr(hand[i], "effect")), intOr(hand[i], "value")};
                if (i > 0) ImGui::SameLine();
                const bool isSelected = static_cast<int>(i) == selectedCard;
                ImGui::SetCursorPosY(rowY - (isSelected ? 15.0f : 0.0f));
                if (drawCard(view, myTurn && myMana >= view.manaCost, isSelected) && myTurn) {
                    selectedCard = isSelected ? -1 : static_cast<int>(i);
                    notice.clear();
                }
            }
            ImGui::EndChild();
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    client.disconnect();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
