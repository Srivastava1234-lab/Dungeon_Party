#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "GameData.h"
#include "NetClient.h"
#include "Protocol.h"
#include "Renderer.h"
#include "RoomServer.h"

using nlohmann::json;

namespace {

std::string projectPath(const std::string& relative) {
#ifdef DUNGEON_PARTY_PROJECT_DIR
    return (std::filesystem::path(DUNGEON_PARTY_PROJECT_DIR) / relative).string();
#else
    return relative;
#endif
}
// An enum cannot be printed using ImGui::text so we need to switch it to its equivalent str/char value
static const char* roleName(LobbyRole role){
    switch (role)
    {
    case LobbyRole::Warrior:
        return "Warrior";

    case LobbyRole::Healer:
        return "Healer";

    case LobbyRole::Wizard:
        return "Wizard";
    
    case LobbyRole::DM:
        return "DM";
    }
    return "";
}

// Start -> Menu (Host or Join) -> Connecting -> Lobby -> Battle
// (Your teammate's Character/Name screen can easily slot between Start and Menu!)
enum class OnlineScreen { Start, Menu, Connecting, Lobby, Battle };

HeroEffect effectFromName(const std::string& name) {
    if (name == "HEAL") return HeroEffect::HEAL;
    if (name == "BUFF") return HeroEffect::BUFF;
    if (name == "SPELL") return HeroEffect::SPELL;
    return HeroEffect::ATTACK;
}

int intOr(const json& j, const char* key, int fallback = 0) {
    auto it = j.find(key);
    return (it != j.end() && it->is_number_integer()) ? it->get<int>() : fallback;
}

std::string strOr(const json& j, const char* key, const std::string& fallback = "") {
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

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
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1000, 650, "Dungeon Party", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load GameData and Textures
    std::unique_ptr<GameData> gameData;
    std::string dataErrorMessage;
    std::map<std::string, GLuint> heroTextures, monsterTextures;
    int roleW = 0, roleH = 0;
    GLuint bgTexture = 0;
    GLuint menuTexture = 0; // Menu screen background
    GLuint warriorTexture = 0, healerTexture = 0; // role cards in the lobby (SCRUM-35)
    LobbyRole myRole = LobbyRole::None;
    int w = 0, h = 0;
    try {
        gameData = std::make_unique<GameData>(GameData::defaultDataDirectory());
        for (const auto& hero : gameData->getHeroes()) {
            GLuint tex = 0;
            if (!hero.image.empty() && LoadTextureFromFile(projectPath(hero.image).c_str(), &tex, &w, &h))
                heroTextures[hero.id] = tex;
        }
        for (const auto& monster : gameData->getMonsters()) {
            GLuint tex = 0;
            if (!monster.image.empty() && LoadTextureFromFile(projectPath(monster.image).c_str(), &tex, &w, &h))
                monsterTextures[monster.name] = tex;
        }
    } catch (const std::exception& e) {
        dataErrorMessage = std::string("Could not load game data:\n") + e.what();
        std::cerr << dataErrorMessage << "\n";
    }
    LoadTextureFromFile(projectPath("assets/Level_1_Artwork.png").c_str(), &bgTexture, &w, &h);
    LoadTextureFromFile(projectPath("assets/Menu_Background_AI_Generated.png").c_str(), &menuTexture, &w, &h);
    LoadTextureFromFile(projectPath("assets/Warrior_Artwork.png").c_str(), &warriorTexture, &roleW, &roleH);
    LoadTextureFromFile(projectPath("assets/Healer_Artwork.png").c_str(), &healerTexture, &roleW, &roleH);

    auto textureOr0 = [](const std::map<std::string, GLuint>& textures, const std::string& key) {
        auto it = textures.find(key);
        return it == textures.end() ? GLuint(0) : it->second;
    };

    // ---- Listen Server & Client state
    std::unique_ptr<RoomServer> localServer; // Only created when this player clicks "Host"
    NetClient client;
    OnlineScreen screen = OnlineScreen::Start;
    json pendingRequest;  // "create" or "join", sent as soon as the connection is up
    std::string notice;   // last error / info line shown to the player
    std::string roomCode, myId, hostId;
    json lobbyPlayers = json::array();
    json state;           // latest match state from the server
    int selectedCard = -1;
    int selectedMonster = -1; // DM only: the monster that will carry out the next queued attack
    bool showHelp = false;    // "?" window on the menu screen

    char nameBuf[net::MAX_NAME_LENGTH + 1] = ""; // empty: Host/Join stay disabled until a name is typed
    char serverBuf[128] = "127.0.0.1";
    int port = net::DEFAULT_PORT;
    char codeBuf[16] = "";

    auto backToMenu = [&](const std::string& why) {
        client.disconnect();
        screen = OnlineScreen::Menu;
        notice = why;
        roomCode.clear();
        myId.clear();
        hostId.clear();
        state = nullptr;
        selectedCard = -1;
        selectedMonster = -1;
    };

    auto startConnecting = [&](const char* targetIp, json request) {
        notice.clear();
        if (!client.connect(targetIp, static_cast<std::uint16_t>(port))) {
            notice = client.lastError();
            return;
        }
        pendingRequest = std::move(request);
        screen = OnlineScreen::Connecting;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ---- 1. Tick embedded Listen Server if this PC is hosting
        if (localServer) {
            localServer->poll(0);
        }

        // ---- 2. Tick Network Client: once per frame, never blocks
        client.poll();
        if (screen != OnlineScreen::Start && screen != OnlineScreen::Menu &&
            client.status() == NetClient::Status::Disconnected) {
            backToMenu(client.lastError().empty() ? "Disconnected" : client.lastError());
        }
        if (screen == OnlineScreen::Connecting && client.status() == NetClient::Status::Connected &&
            !pendingRequest.is_null()) {
            client.send(pendingRequest);
            pendingRequest = nullptr;
        }
        for (const json& message : client.takeMessages()) {
            const std::string type = strOr(message, "t");
            if (type == net::msg::JOINED) {
                roomCode = strOr(message, "code");
                myId = strOr(message, "you");
                myRole = LobbyRole::None;
                screen = OnlineScreen::Lobby;
            } else if (type == net::msg::LOBBY) {
                roomCode = strOr(message, "code", roomCode);
                hostId = strOr(message, "host");
                lobbyPlayers = message.value("players", json::array());
                // The server decides who is the DM; the picker follows what it says.
                for (const auto& p : lobbyPlayers) {
                    if (strOr(p, "id") != myId) continue;
                    if (strOr(p, "role") == "DM") myRole = LobbyRole::DM;
                    else if (myRole == LobbyRole::DM) myRole = LobbyRole::None;
                }
                screen = OnlineScreen::Lobby;
            } else if (type == net::msg::STATE) {
                state = message;
                hostId = strOr(message, "host", hostId);
                screen = OnlineScreen::Battle;
            } else if (type == net::msg::ERROR) {
                notice = strOr(message, "msg");
                if (screen == OnlineScreen::Connecting) backToMenu(notice);
            } else if (type == net::msg::CLOSED) {
                backToMenu(strOr(message, "msg", "The room was closed"));
            }
        }

        glClearColor(0.12f, 0.34f, 0.56f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // The menu picture is the background of the Menu screen only (as in SCRUM-32).
        if (menuTexture && (screen == OnlineScreen::Menu || screen == OnlineScreen::Connecting)) {
            ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)menuTexture, ImVec2(0, 0),
                                                     ImGui::GetIO().DisplaySize);
        }

        // ---- SCREEN 1: Start Screen
        if (screen == OnlineScreen::Start) {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 220));
            ImGui::Begin("Start", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::TextUnformatted("DUNGEON PARTY");
            ImGui::SetWindowFontScale(1.0f);

            if (!dataErrorMessage.empty()) {
                ImGui::TextWrapped("%s", dataErrorMessage.c_str());
            } else if (ImGui::Button("Start match", ImVec2(-1, 70))) {
                screen = OnlineScreen::Menu;
            }
            ImGui::End();
        }
        // ---- SCREEN 2: Host / Join Menu (SCRUM-32 layout: name on top, Host scroll left, Join scroll right)
        else if (screen == OnlineScreen::Menu || screen == OnlineScreen::Connecting) {
            const ImVec2 win = ImGui::GetIO().DisplaySize;
            const ImGuiWindowFlags fixedFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

            if (screen == OnlineScreen::Connecting) {
                ImGui::SetNextWindowPos(ImVec2(win.x * 0.5f, win.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(420, 110));
                ImGui::Begin("Connecting", nullptr, fixedFlags);
                ImGui::Text("Connecting to %s:%d ...", serverBuf, port);
                if (ImGui::Button("Cancel", ImVec2(-1, 0))) backToMenu("");
                ImGui::End();
            } else {
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(win);
                ImGui::Begin("Menu", nullptr, fixedFlags | ImGuiWindowFlags_NoBackground);

                if (ImGui::Button("Back")) {
                    notice.clear();
                    screen = OnlineScreen::Start;
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                if (ImGui::Button("?")) showHelp = !showHelp;

                // Name field, centred above the two scrolls
                ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.2f));
                ImGui::SetNextItemWidth(win.x * 0.3f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.93f, 0.85f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.25f, 0.15f, 0.05f, 1.0f));
                ImGui::InputTextWithHint("##name", "Your name", nameBuf, sizeof(nameBuf));
                ImGui::PopStyleColor(2);
                // Host and Join only work with a name (a C string is empty when its first char is '\0').
                const bool nameFilled = nameBuf[0] != '\0';

                if (!notice.empty()) {
                    ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.26f));
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
                }

                // The two panels sit inside the empty parchment scrolls of Menu_Background_AI_Generated.png
                const float panelW = win.x * 0.235f;
                const float panelH = win.y * 0.40f;
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.25f, 0.15f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.85f, 0.70f, 0.40f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.80f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.75f, 0.60f, 0.30f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.93f, 0.85f, 0.65f, 1.0f));

                // Left panel -- Hosting -- (starts the embedded listen server automatically)
                ImGui::SetCursorPos(ImVec2(win.x * 0.07f, win.y * 0.35f));
                ImGui::BeginChild("Host", ImVec2(panelW, panelH), true);
                ImGui::Text("HOST A PARTY");
                ImGui::TextWrapped("Create a room and share its code with your friends.");
                ImGui::BeginDisabled(!nameFilled);
                if (ImGui::Button("New Adventure", ImVec2(-1, 50))) {
                    if (!localServer && gameData) {
                        try {
                            localServer = std::make_unique<RoomServer>(*gameData, static_cast<std::uint16_t>(port));
                        } catch (const std::exception& e) {
                            // If another window on this same PC is already hosting on port 7777,
                            // we can still connect to 127.0.0.1 and create a room on it!
                            std::cerr << "Local server note: " << e.what() << "\n";
                        }
                    }
                    std::snprintf(serverBuf, sizeof(serverBuf), "127.0.0.1");
                    startConnecting("127.0.0.1", {{"t", net::msg::CREATE}, {"name", nameBuf}});
                }
                ImGui::EndDisabled();
                ImGui::EndChild();

                // Right panel -- Joining -- (host's IP and port are needed for the network)
                ImGui::SetCursorPos(ImVec2(win.x * 0.71f, win.y * 0.35f));
                ImGui::BeginChild("Join", ImVec2(panelW, panelH), true);
                ImGui::Text("JOIN A PARTY");
                ImGui::TextWrapped("Enter the host's IP and the code you got from the host:");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##hostip", "Host IP", serverBuf, sizeof(serverBuf));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##port", &port);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##code", "Room code", codeBuf, sizeof(codeBuf),
                                         ImGuiInputTextFlags_CharsUppercase);
                ImGui::BeginDisabled(!nameFilled);
                if (ImGui::Button("Join", ImVec2(-1, 50))) {
                    startConnecting(serverBuf, {{"t", net::msg::JOIN}, {"name", nameBuf}, {"code", codeBuf}});
                }
                ImGui::EndDisabled();
                ImGui::EndChild();

                ImGui::PopStyleColor(5); // every pushed color must be popped
                ImGui::End();

                if (showHelp) {
                    ImGui::Begin("Help", &showHelp);
                    ImGui::TextWrapped("Host a room to get a code and share it with your friends. "
                                       "Or enter the host's IP and their code on the right to join their room.");
                    ImGui::End();
                }
            }
        }
        // ---- SCREEN 3: Waiting Room Lobby
        else if (screen == OnlineScreen::Lobby) {
            beginFullscreenWindow("Lobby");
            drawRoomCode(roomCode, 2.0f);
            ImGui::Separator();
            ImGui::Text("Give this code to your friends. Players (%zu/%zu):",
                        lobbyPlayers.size(), net::MAX_PLAYERS_PER_ROOM);
            for (const auto& p : lobbyPlayers) {
                const std::string id = strOr(p, "id");
                ImGui::BulletText("%s%s%s%s", strOr(p, "name").c_str(),
                                  strOr(p, "role") == "DM" ? "  [Dungeon Master]" : "",
                                  id == hostId ? "  (host)" : "",
                                  id == myId ? "  <- you" : "");
            }
            ImGui::Spacing();

            // ROLE CARDS (SCRUM-35, Soroush). DM is sent to the server, which allows only one DM per room;
            // every other card is a hero (the server still plays the warrior for all heroes).
            ImGui::Text("Your role: %s", roleName(myRole));
            bool dmTaken = false; // another player already holds the DM role
            for (const auto& p : lobbyPlayers)
                if (strOr(p, "role") == "DM" && strOr(p, "id") != myId) dmTaken = true;
            const LobbyRole roles[4] = {LobbyRole::Warrior, LobbyRole::Wizard, LobbyRole::Healer, LobbyRole::DM};
            for (LobbyRole role : roles) {
                const bool chosen = (role == myRole);
                const bool locked = (role == LobbyRole::DM && dmTaken);
                ImGui::BeginDisabled(locked);
                GLuint tex = role == LobbyRole::Warrior ? warriorTexture : role == LobbyRole::Healer ? healerTexture : 0;
                if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 1.0f));
                const bool clicked = tex ? ImGui::ImageButton(roleName(role), (ImTextureID)(intptr_t)tex, ImVec2(120, 120))
                                         : ImGui::Button(roleName(role), ImVec2(120, 120));
                if (chosen) ImGui::PopStyleColor();
                ImGui::EndDisabled();
                if (clicked) {
                    if (role == LobbyRole::DM) {
                        client.send({{"t", net::msg::ROLE}, {"role", "DM"}}); // highlighted once the server confirms
                    } else {
                        myRole = role;
                        client.send({{"t", net::msg::ROLE}, {"role", "HERO"}});
                    }
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::Spacing();

            if (myId == hostId) {
                if (ImGui::Button("Start match", ImVec2(200, 40)))
                    client.send({{"t", net::msg::START}});
            } else {
                ImGui::TextUnformatted("Waiting for the host to start...");
            }
            ImGui::SameLine();
            if (ImGui::Button("Leave", ImVec2(100, 40))) backToMenu("");
            if (!notice.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
            ImGui::End();

        }
        // ---- SCREEN 4: Battle Screen
        else if (screen == OnlineScreen::Battle && state.is_object()) {
            beginFullscreenWindow("Battle");
            const bool myTurn = !state["active"].is_null() && strOr(state, "active") == myId;
            const bool isDm = strOr(state, "role") == "DM"; // I am the Dungeon Master in this match

            // Top bar: room code top-left, then turn info
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
                ImGui::Text("Round %d | %s", intOr(state, "round"),
                            myTurn ? "YOUR TURN" : (whose + "'s turn").c_str());
                if (myTurn) {
                    ImGui::SameLine();
                    if (ImGui::Button("End turn")) {
                        client.send({{"t", net::msg::END}});
                        selectedCard = -1;
                        selectedMonster = -1;
                        notice.clear(); // an old hint/error must not stay on screen into the next turn
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Leave")) backToMenu("");
            ImGui::EndChild();

            // Battlefield
            ImGui::BeginChild("Battlefield", ImVec2(0, ImGui::GetWindowHeight() - (isDm ? 260 : 230)), true);
            if (bgTexture) {
                ImGui::SetCursorPos(ImVec2(0, 0));
                ImGui::Image((ImTextureID)(intptr_t)bgTexture, ImGui::GetContentRegionAvail());
                ImGui::SetCursorPos(ImVec2(10, 10));
            }
            int drawn = 0;
            const json& heroList = state["heroes"];
            for (std::size_t hi = 0; hi < heroList.size(); ++hi) {
                const auto& hero = heroList[hi];
                if (drawn++ > 0) ImGui::SameLine();
                std::string label = strOr(hero, "name");
                if (strOr(hero, "id") == myId) label += " (you)";
                if (!hero.value("connected", true)) label += " [left]";
                const bool heroClicked = drawCharacter({label, intOr(hero, "hp"), intOr(hero, "maxHp", 1),
                               intOr(hero, "mana"), intOr(hero, "maxMana"),
                               textureOr0(heroTextures, strOr(hero, "heroId"))});
                // DM: a hero is the target of the selected card (curse now, or a queued monster attack)
                const json dmHand = state.value("hand", json::array());
                if (heroClicked && isDm && myTurn && selectedCard >= 0 && selectedCard < static_cast<int>(dmHand.size())) {
                    const std::string effect = strOr(dmHand[selectedCard], "effect");
                    if (effect == "WEAKNESS") {
                        client.send({{"t", net::msg::PLAY}, {"card", selectedCard}, {"target", hi}});
                        selectedCard = -1;
                    } else if (effect == "ATTACK") {
                        if (selectedMonster < 0) {
                            notice = "Click a monster first, then the hero it should hit";
                        } else {
                            client.send({{"t", net::msg::QUEUE}, {"card", selectedCard},
                                         {"monster", selectedMonster}, {"target", hi}});
                            selectedCard = -1;
                            selectedMonster = -1;
                        }
                    }
                }
            }
            ImGui::SameLine(0, 60);
            drawn = 0;
            const json& monsters = state["monsters"];
            for (std::size_t i = 0; i < monsters.size(); ++i) {
                if (!monsters[i].value("alive", false)) continue;
                if (drawn++ > 0) ImGui::SameLine();
                const bool clicked = drawCharacter({
                    strOr(monsters[i], "name"),
                    intOr(monsters[i], "hp"),
                    intOr(monsters[i], "maxHp", 1),
                    0, 0,
                    textureOr0(monsterTextures, strOr(monsters[i], "name"))
                });
                if (isDm) {
                    // DM: pick which monster carries out the attack card that is selected
                    const json dmHand = state.value("hand", json::array());
                    if (clicked && myTurn && selectedCard >= 0 && selectedCard < static_cast<int>(dmHand.size()) &&
                        strOr(dmHand[selectedCard], "effect") == "ATTACK") {
                        selectedMonster = static_cast<int>(i);
                        notice.clear();
                    }
                } else if (clicked && myTurn && selectedCard >= 0) {
                    client.send({{"t", net::msg::PLAY}, {"card", selectedCard}, {"target", i}});
                    selectedCard = -1;
                }
            }
            ImGui::EndChild();

            // Hand
            ImGui::BeginChild("Hand", ImVec2(0, 0), true);
            if (isDm) {
                // ---- DM view (SCRUM-40): DM mana, deck, hand, and the attacks queued this turn
                const json dmInfo = state.value("dm", json::object());
                const int dmMana = intOr(dmInfo, "mana");
                ImGui::Text("Dungeon Master  |  Mana %d / %d  |  Deck %d  |  Hand %d  |  Discard %d  |  Cards this turn %d / 3",
                            dmMana, intOr(dmInfo, "maxMana"), intOr(dmInfo, "deckSize"),
                            static_cast<int>(state.value("hand", json::array()).size()),
                            intOr(dmInfo, "discardSize"), intOr(state, "cardsPlayed"));

                // Queued attacks (only the DM is ever sent these)
                auto nameAt = [&](const char* list, const json& index) -> std::string {
                    if (!index.is_number_integer()) return "?";
                    const json& items = state[list];
                    const int at = index.get<int>();
                    return (at >= 0 && at < static_cast<int>(items.size())) ? strOr(items[at], "name") : "?";
                };
                std::string queuedText;
                for (const auto& q : dmInfo.value("queued", json::array())) {
                    if (!queuedText.empty()) queuedText += ",  ";
                    queuedText += nameAt("monsters", q["monster"]) + " -> " + nameAt("heroes", q["target"]) +
                                  " (" + strOr(q, "card") + ")";
                }
                ImGui::Text("Queued attacks: %s", queuedText.empty() ? "none" : queuedText.c_str());

                if (!notice.empty()) {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
                } else if (myTurn && selectedCard >= 0) {
                    const json dmHand = state.value("hand", json::array());
                    const std::string effect = (selectedCard < static_cast<int>(dmHand.size()))
                                                   ? strOr(dmHand[selectedCard], "effect") : "";
                    if (effect == "ATTACK" && selectedMonster < 0) ImGui::TextUnformatted("Now click the monster that attacks.");
                    else if (effect == "ATTACK") ImGui::Text("%s attacks - now click the hero to hit.", nameAt("monsters", selectedMonster).c_str());
                    else ImGui::TextUnformatted("Now click the hero to curse.");
                } else if (myTurn) {
                    ImGui::TextUnformatted("Your turn: pick a card.");
                } else {
                    ImGui::TextUnformatted("Waiting for the heroes...");
                }

                const json dmHand = state.value("hand", json::array());
                for (std::size_t i = 0; i < dmHand.size(); ++i) {
                    if (i > 0) ImGui::SameLine();
                    const bool isSelected = static_cast<int>(i) == selectedCard;
                    const int cost = intOr(dmHand[i], "cost");
                    const bool playable = myTurn && dmMana >= cost && intOr(state, "cardsPlayed") < 3;
                    const std::string effect = strOr(dmHand[i], "effect");
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::BeginDisabled(!playable);
                    if (isSelected) ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 220, 90, 255));
                    ImGui::BeginChild("dmcard", ImVec2(150, 80), true);
                    ImGui::Text("%s", strOr(dmHand[i], "name").c_str());
                    ImGui::Text("Cost: %d", cost);
                    if (effect == "ATTACK") ImGui::Text("Monster hits for %d", intOr(dmHand[i], "value"));
                    else if (effect == "WEAKNESS") ImGui::Text("Hero attack -%d", intOr(dmHand[i], "value"));
                    else ImGui::Text("%s %d", effect.c_str(), intOr(dmHand[i], "value"));
                    ImGui::EndChild();
                    const bool cardClicked = ImGui::IsItemClicked();
                    if (isSelected) ImGui::PopStyleColor();
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    if (cardClicked && myTurn) {
                        selectedCard = isSelected ? -1 : static_cast<int>(i);
                        selectedMonster = -1;
                        notice.clear();
                    }
                }
            } else {
            int myMana = 0;
            for (const auto& hero : state["heroes"])
                if (strOr(hero, "id") == myId) myMana = intOr(hero, "mana");
            if (!notice.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
            const float rowY = ImGui::GetCursorPosY() + 15.0f;
            const json hand = state.value("hand", json::array());
            for (std::size_t i = 0; i < hand.size(); ++i) {
                CardView view{
                    strOr(hand[i], "name"),
                    intOr(hand[i], "cost"),
                    effectFromName(strOr(hand[i], "effect")),
                    intOr(hand[i], "value")
                };
                if (i > 0) ImGui::SameLine();
                const bool isSelected = static_cast<int>(i) == selectedCard;
                ImGui::SetCursorPosY(rowY - (isSelected ? 15.0f : 0.0f));
                ImGui::PushID(static_cast<int>(i)); // two copies of a card (e.g. 2x Strike) need different ImGui IDs
                const bool cardClicked = drawCard(view, myTurn && myMana >= view.manaCost, isSelected);
                ImGui::PopID();
                if (cardClicked && myTurn) {
                    selectedCard = isSelected ? -1 : static_cast<int>(i);
                    notice.clear();
                }
            }
            } // end of hero hand
            ImGui::EndChild();
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    client.disconnect();
    localServer.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}