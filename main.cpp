// Dungeon Party - game client
//
// Start -> Menu (Host or Join) -> Connecting -> Lobby (pick a role) -> Battle
//
// All game state comes from a RoomServer. "Host on this computer" starts one inside this
// program (a listen server); "Host on a server" uses a dungeon_server running somewhere else,
// e.g. the spare laptop. Players on OTHER networks can only reach a server that is reachable
// from the internet (port forwarding or Tailscale) - see net/PROTOCOL.md.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
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

// ---- Roles (SCRUM-35). The server only knows role ids: "warrior", "healer" (hero ids) and "dm".

const char* roleName(LobbyRole role) {
    switch (role) {
        case LobbyRole::Warrior: return "Warrior";
        case LobbyRole::Healer:  return "Healer";
        case LobbyRole::Wizard:  return "Wizard";
        case LobbyRole::DM:      return "Dungeon Master";
        case LobbyRole::None:    return "not chosen";
    }
    return "";
}

std::string roleId(LobbyRole role) {
    switch (role) {
        case LobbyRole::Warrior: return "warrior";
        case LobbyRole::Healer:  return "healer";
        case LobbyRole::DM:      return net::DM_ROLE;
        default:                 return "";
    }
}

LobbyRole roleFromId(const std::string& id) {
    if (id == "warrior") return LobbyRole::Warrior;
    if (id == "healer") return LobbyRole::Healer;
    if (id == net::DM_ROLE) return LobbyRole::DM;
    return LobbyRole::None;
}

enum class OnlineScreen { Start, Menu, Connecting, Lobby, Battle };

HeroEffect effectFromName(const std::string& name) {
    if (name == "HEAL") return HeroEffect::HEAL;
    if (name == "BUFF") return HeroEffect::BUFF;
    if (name == "SPELL") return HeroEffect::SPELL;
    if (name == "SHIELD") return HeroEffect::SHIELD;
    if (name == "FIREBALL") return HeroEffect::FIREBALL;
    return HeroEffect::ATTACK;
}

// Cards that are played on a hero instead of a monster
bool targetsHero(const std::string& effect) { return effect == "HEAL" || effect == "SHIELD"; }

// Safe readers: the state comes over the network, so a missing field must not crash the client.
int intOr(const json& j, const char* key, int fallback = 0) {
    auto it = j.find(key);
    return (it != j.end() && it->is_number_integer()) ? it->get<int>() : fallback;
}
std::string strOr(const json& j, const char* key, const std::string& fallback = "") {
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : fallback;
}
bool boolOr(const json& j, const char* key, bool fallback = false) {
    auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}

bool isThisComputer(const std::string& address) {
    return address.empty() || address == "127.0.0.1" || address == "localhost";
}

void drawRoomCode(const std::string& code, float fontScale) {
    ImGui::SetWindowFontScale(fontScale);
    ImGui::TextUnformatted(("Room " + code).c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(code.c_str());
}

void beginFullscreenWindow(const char* name, bool transparent = false) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    if (transparent) ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
}

// Draws a picture behind every window, stretched to the whole screen.
void drawBackground(GLuint texture) {
    if (!texture) return;
    ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)texture, ImVec2(0, 0), ImGui::GetIO().DisplaySize);
}

void drawNotice(const std::string& notice) {
    if (!notice.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
}

// Help popup, opened with the Help_Button.png button on the start and menu screens.
void drawHelpPopup() {
    ImGui::SetNextWindowSize(ImVec2(560, 0));
    if (ImGui::BeginPopupModal("How to play", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Hosting");
        ImGui::BulletText("This computer: the game runs the server itself. Friends on the SAME\n"
                          "network join with the address shown in your lobby.");
        ImGui::BulletText("A server: enter the address of a computer running dungeon_server\n"
                          "(for example the team laptop). Use this to play across different networks.");
        ImGui::BulletText("Different networks only work if that server is reachable: port\n"
                          "forwarding of UDP 7777 on its router, or everyone in the same Tailscale network.");
        ImGui::Spacing();
        ImGui::TextWrapped("Playing");
        ImGui::BulletText("Heroes: pick a card, then click a monster (or a hero for Heal).");
        ImGui::BulletText("Dungeon Master: pick an attack card, click a monster, then the hero it\n"
                          "should attack. Orders happen when you press End turn.");
        ImGui::BulletText("The final boss arrives in a later round. Defeat it to win.");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
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

    GLFWwindow* window = glfwCreateWindow(1280, 760, "Dungeon Party", nullptr, nullptr);
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

    // ---- Game data and artwork. Every picture path comes from the data files or assets/.
    std::unique_ptr<GameData> gameData;
    std::string dataErrorMessage;
    std::map<std::string, GLuint> heroTextures;    // by hero id ("warrior", "healer")
    std::map<std::string, GLuint> monsterTextures; // by monster name ("Goblin", "Final Boss")
    std::map<std::string, GLuint> cardTextures;    // by card name ("Strike", "Heal", ...)
    int w = 0, h = 0;
    auto loadTexture = [&](const std::string& relativePath) {
        GLuint tex = 0;
        if (!relativePath.empty()) LoadTextureFromFile(projectPath(relativePath).c_str(), &tex, &w, &h);
        return tex;
    };
    try {
        gameData = std::make_unique<GameData>(GameData::defaultDataDirectory());
        for (const auto& hero : gameData->getHeroes())
            if (GLuint tex = loadTexture(hero.image)) heroTextures[hero.id] = tex;
        for (const auto& monster : gameData->getMonsters())
            if (GLuint tex = loadTexture(monster.image)) monsterTextures[monster.name] = tex;
        for (const auto& card : gameData->getCards())
            if (GLuint tex = loadTexture(card.image)) cardTextures[card.name] = tex;
    } catch (const std::exception& e) {
        dataErrorMessage = std::string("Could not load game data:\n") + e.what();
        std::cerr << dataErrorMessage << "\n";
    }
    const GLuint battleBackground = loadTexture("assets/Level_1_Artwork.png");
    const GLuint menuBackground = loadTexture("assets/Menu_Background_AI_Generated.png");
    const GLuint helpButton = loadTexture("assets/Help_Button.png");
    const GLuint heroesWinPicture = loadTexture("assets/Heroes_Win.png");
    const GLuint heroesLosePicture = loadTexture("assets/Heroes_Lose.png");
    const GLuint emptyCardPicture = loadTexture("assets/Empty_Card_Design.png"); // cards without their own art

    auto textureOr0 = [](const std::map<std::string, GLuint>& textures, const std::string& key) {
        auto it = textures.find(key);
        return it == textures.end() ? GLuint(0) : it->second;
    };
    auto roleTexture = [&](LobbyRole role) -> GLuint {
        if (role == LobbyRole::Warrior) return textureOr0(heroTextures, "warrior");
        if (role == LobbyRole::Healer) return textureOr0(heroTextures, "healer");
        if (role == LobbyRole::DM) return textureOr0(monsterTextures, "Final Boss");
        return 0;
    };

    // ---- Network state
    std::unique_ptr<RoomServer> localServer; // only created when this player hosts on this computer
    NetClient client;
    OnlineScreen screen = OnlineScreen::Start;
    json pendingRequest;  // "create" or "join", sent as soon as the connection is up
    std::string notice;   // last error / info line shown to the player
    std::string roomCode, myId, hostId;
    json lobbyPlayers = json::array();
    json state;           // latest match state from the server
    const std::string localAddress = client.localNetworkAddress();

    char nameBuf[net::MAX_NAME_LENGTH + 1] = "Player";
    char serverBuf[128] = "";       // address to JOIN
    char hostServerBuf[128] = "";   // address of a dedicated server to HOST on
    int hostWhere = 0;              // 0 = this computer, 1 = a server
    int port = net::DEFAULT_PORT;
    char codeBuf[16] = "";

    // ---- Battle selection: what the player has clicked so far
    int selectedCard = -1;     // hand index
    int selectedMonster = -1;  // DM: the monster that will attack
    bool bossOrder = false;    // DM: choosing a target for the final boss
    std::vector<std::string> dmOrders; // DM: orders accepted this turn (shown as a list)
    std::string pendingOrder;          // DM: order sent, waiting for the server to accept it
    std::string lastActive;

    auto clearSelection = [&] {
        selectedCard = -1;
        selectedMonster = -1;
        bossOrder = false;
    };

    auto backToMenu = [&](const std::string& why) {
        client.disconnect();
        screen = OnlineScreen::Menu;
        notice = why;
        roomCode.clear(); myId.clear(); hostId.clear();
        state = nullptr;
        clearSelection();
        dmOrders.clear(); pendingOrder.clear(); lastActive.clear();
    };

    auto startConnecting = [&](const std::string& address, json request) {
        notice.clear();
        if (!client.connect(address, static_cast<std::uint16_t>(port))) {
            notice = client.lastError();
            return;
        }
        pendingRequest = std::move(request);
        screen = OnlineScreen::Connecting;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ---- 1. Run the embedded server if this computer is hosting
        if (localServer) localServer->poll(0);

        // ---- 2. Network client: once per frame, never blocks
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
                screen = OnlineScreen::Lobby;
                notice.clear();
            } else if (type == net::msg::LOBBY) {
                roomCode = strOr(message, "code", roomCode);
                hostId = strOr(message, "host");
                lobbyPlayers = message.value("players", json::array());
                screen = OnlineScreen::Lobby;
            } else if (type == net::msg::STATE) {
                state = message;
                hostId = strOr(message, "host", hostId);
                screen = OnlineScreen::Battle;
                const std::string active = message["active"].is_string() ? message["active"].get<std::string>() : "";
                if (active != lastActive) { // a new turn started: forget the old selection and orders
                    lastActive = active;
                    clearSelection();
                    dmOrders.clear();
                    pendingOrder.clear();
                }
                if (!pendingOrder.empty()) { dmOrders.push_back(pendingOrder); pendingOrder.clear(); }
            } else if (type == net::msg::ERROR) {
                notice = strOr(message, "msg");
                pendingOrder.clear();
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
        ImGuiIO& io = ImGui::GetIO();

        // ================= SCREEN 1: Start =================
        if (screen == OnlineScreen::Start) {
            drawBackground(menuBackground);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 0));
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::Begin("Start", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::TextUnformatted("DUNGEON PARTY");
            ImGui::SetWindowFontScale(1.0f);
            if (!dataErrorMessage.empty()) {
                ImGui::TextWrapped("%s", dataErrorMessage.c_str());
            } else if (ImGui::Button("Play", ImVec2(-1, 70))) {
                screen = OnlineScreen::Menu;
            }
            ImGui::End();
        }
        // ================= SCREEN 2: Host / Join =================
        else if (screen == OnlineScreen::Menu || screen == OnlineScreen::Connecting) {
            drawBackground(menuBackground);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(500, 0));
            ImGui::SetNextWindowBgAlpha(0.9f);
            ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::TextUnformatted("MULTIPLAYER");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SameLine(ImGui::GetWindowWidth() - 52);
            const bool helpClicked = helpButton ? ImGui::ImageButton("help", (ImTextureID)(intptr_t)helpButton, ImVec2(28, 28))
                                                : ImGui::Button("?", ImVec2(36, 36));
            if (helpClicked) ImGui::OpenPopup("How to play");
            drawHelpPopup();
            ImGui::Separator();

            if (screen == OnlineScreen::Connecting) {
                ImGui::Text("Connecting ...");
                if (ImGui::Button("Cancel", ImVec2(-1, 0))) backToMenu("");
            } else {
                ImGui::InputText("Your name", nameBuf, sizeof(nameBuf));
                ImGui::InputInt("Port", &port);
                ImGui::Spacing();
                ImGui::Separator();

                // ---- Host
                ImGui::TextUnformatted("Host a new room on:");
                ImGui::RadioButton("This computer", &hostWhere, 0);
                ImGui::SameLine();
                ImGui::RadioButton("A server", &hostWhere, 1);
                if (hostWhere == 1)
                    ImGui::InputTextWithHint("Server address##host", "e.g. 100.101.102.103 or team.duckdns.org",
                                             hostServerBuf, sizeof(hostServerBuf));
                else
                    ImGui::TextDisabled("Friends on other networks can't reach this computer\n"
                                        "unless your router forwards UDP %d to it (see ?).", port);

                if (ImGui::Button("Host", ImVec2(-1, 40))) {
                    const json create = {{"t", net::msg::CREATE}, {"name", nameBuf}};
                    if (hostWhere == 1 && !isThisComputer(hostServerBuf)) {
                        startConnecting(hostServerBuf, create); // a dedicated dungeon_server somewhere else
                    } else {
                        if (!localServer && gameData) {
                            try {
                                localServer = std::make_unique<RoomServer>(*gameData, static_cast<std::uint16_t>(port));
                            } catch (const std::exception& e) {
                                // Port already taken (e.g. a second window on this computer is hosting):
                                // we can still create a room on that server.
                                std::cerr << "Local server note: " << e.what() << "\n";
                            }
                        }
                        startConnecting("127.0.0.1", create);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();

                // ---- Join
                ImGui::TextUnformatted("Join a room:");
                ImGui::InputTextWithHint("Server address##join", "the address the host gave you", serverBuf, sizeof(serverBuf));
                ImGui::InputTextWithHint("##code", "Room code (e.g. K7RM2Q)", codeBuf, sizeof(codeBuf), ImGuiInputTextFlags_CharsUppercase);
                ImGui::SameLine();
                if (ImGui::Button("Join", ImVec2(-1, 0))) {
                    const std::string address = isThisComputer(serverBuf) ? "127.0.0.1" : serverBuf;
                    startConnecting(address, {{"t", net::msg::JOIN}, {"name", nameBuf}, {"code", codeBuf}});
                }
            }
            drawNotice(notice);
            ImGui::End();
        }
        // ================= SCREEN 3: Lobby =================
        else if (screen == OnlineScreen::Lobby) {
            drawBackground(menuBackground);
            beginFullscreenWindow("Lobby", true);
            drawRoomCode(roomCode, 2.0f); // top-left
            if (localServer) {
                ImGui::TextDisabled("Hosted on this computer. Same network: server address %s. Other networks: see ? in the menu.",
                                    localAddress.empty() ? "(unknown - run ipconfig)" : localAddress.c_str());
            }
            ImGui::Separator();

            // Players and the roles the SERVER has confirmed
            LobbyRole myRole = LobbyRole::None;
            std::string dmTakenBy;
            ImGui::Text("Players (%zu/%zu):", lobbyPlayers.size(), net::MAX_PLAYERS_PER_ROOM);
            for (const auto& p : lobbyPlayers) {
                const std::string id = strOr(p, "id");
                const LobbyRole role = roleFromId(strOr(p, "role"));
                if (id == myId) myRole = role;
                if (role == LobbyRole::DM && id != myId) dmTakenBy = strOr(p, "name");
                ImGui::BulletText("%s - %s%s%s", strOr(p, "name").c_str(), roleName(role),
                                  id == hostId ? "  (host)" : "", id == myId ? "  <- you" : "");
            }
            ImGui::Spacing();

            // ROLE CARDS (SCRUM-35, Soroush): clicking sends the role to the server.
            ImGui::Text("Choose your role:");
            const LobbyRole roles[4] = {LobbyRole::Warrior, LobbyRole::Healer, LobbyRole::Wizard, LobbyRole::DM};
            for (LobbyRole role : roles) {
                const bool noHeroData = role == LobbyRole::Wizard; // no wizard in heroes.json yet
                const bool locked = noHeroData || (role == LobbyRole::DM && !dmTakenBy.empty());
                const bool chosen = role == myRole;
                ImGui::BeginGroup();
                ImGui::BeginDisabled(locked);
                if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 1.0f));
                const GLuint tex = roleTexture(role);
                const bool clicked = tex ? ImGui::ImageButton(roleName(role), (ImTextureID)(intptr_t)tex, ImVec2(120, 140))
                                         : ImGui::Button(roleName(role), ImVec2(128, 148));
                if (chosen) ImGui::PopStyleColor();
                ImGui::EndDisabled();
                ImGui::TextUnformatted(roleName(role));
                if (noHeroData) ImGui::TextDisabled("coming soon");
                else if (role == LobbyRole::DM && !dmTakenBy.empty()) ImGui::TextDisabled("taken by %s", dmTakenBy.c_str());
                ImGui::EndGroup();
                if (clicked && !chosen) client.send({{"t", net::msg::ROLE}, {"role", roleId(role)}});
                ImGui::SameLine(0, 18);
            }
            ImGui::NewLine();
            ImGui::Spacing();

            if (myId == hostId) {
                if (ImGui::Button("Start match", ImVec2(200, 40))) client.send({{"t", net::msg::START}});
                ImGui::SameLine();
                ImGui::TextDisabled("No DM chosen = the computer plays the DM.");
            } else {
                ImGui::TextUnformatted("Waiting for the host to start...");
            }
            if (ImGui::Button("Leave", ImVec2(100, 32))) backToMenu("");
            drawNotice(notice);
            ImGui::End();
        }
        // ================= SCREEN 4: Battle =================
        else if (screen == OnlineScreen::Battle && state.is_object()) {
            beginFullscreenWindow("Battle");
            const bool iAmDm = strOr(state, "role") == "dm";
            const bool myTurn = state["active"].is_string() && strOr(state, "active") == myId;
            const bool finished = !state["winner"].is_null();
            const json heroes = state.value("heroes", json::array());
            const json monsters = state.value("monsters", json::array());
            const json hand = state.value("hand", json::array());
            const json dm = state.value("dm", json::object());

            std::string activeName = "Dungeon Master";
            for (const auto& hero : heroes)
                if (strOr(hero, "id") == strOr(state, "active")) activeName = strOr(hero, "name");
            if (strOr(dm, "id") == strOr(state, "active")) activeName = strOr(dm, "name");

            // ---- Top bar: room code top-left, then turn info
            ImGui::BeginChild("TopBar", ImVec2(0, 44), true);
            drawRoomCode(roomCode, 1.4f);
            ImGui::SameLine(230);
            ImGui::Text("Round %d | %s", intOr(state, "round"), myTurn ? "YOUR TURN" : (activeName + "'s turn").c_str());
            if (myTurn && !finished) {
                ImGui::SameLine();
                if (ImGui::Button("End turn")) { client.send({{"t", net::msg::END}}); clearSelection(); }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("| DM: %s%s", strOr(dm, "name").c_str(), iAmDm ? " (you)" : "");
            if (iAmDm && dm.contains("mana")) { ImGui::SameLine(); ImGui::Text("Mana %d/%d", intOr(dm, "mana"), intOr(dm, "maxMana")); }
            ImGui::SameLine(io.DisplaySize.x - 70);
            if (ImGui::SmallButton("Leave")) backToMenu("");
            ImGui::EndChild();

            // ---- What to click next
            std::string selectedEffect = (selectedCard >= 0 && selectedCard < (int)hand.size()) ? strOr(hand[selectedCard], "effect") : "";
            std::string instruction;
            if (!myTurn) instruction = "Waiting for " + activeName + "...";
            else if (iAmDm) {
                if (bossOrder) instruction = "Click the hero the FINAL BOSS should attack.";
                else if (selectedCard < 0) instruction = "Pick an attack card (or Boss attack), then a monster, then the hero it attacks. Orders happen on End turn.";
                else if (selectedMonster < 0) instruction = "Now click the monster that should attack.";
                else instruction = "Now click the hero " + strOr(monsters[selectedMonster], "name") + " should attack.";
            } else {
                if (selectedCard < 0) instruction = "Pick a card from your hand.";
                else if (targetsHero(selectedEffect)) instruction = "Click the hero to use it on.";
                else instruction = "Click a monster to attack.";
            }
            ImGui::TextUnformatted(instruction.c_str());
            if (iAmDm && myTurn && state.value("canBossAttack", false)) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Boss attack")) { clearSelection(); bossOrder = true; }
            }
            if (iAmDm && !dmOrders.empty()) {
                std::string list;
                for (const auto& order : dmOrders) list += (list.empty() ? "" : ", ") + order;
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1), "Orders: %s", list.c_str());
            }
            drawNotice(notice);

            // ---- Battlefield
            ImGui::BeginChild("Battlefield", ImVec2(0, ImGui::GetContentRegionAvail().y - 222), true, ImGuiWindowFlags_NoScrollbar);
            const ImVec2 fieldSize = ImGui::GetContentRegionAvail();
            if (battleBackground) {
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)battleBackground, origin,
                                                     ImVec2(origin.x + fieldSize.x, origin.y + fieldSize.y));
            }
            int livingMonsters = 0;
            for (const auto& m : monsters) if (boolOr(m, "alive")) ++livingMonsters;
            const int figures = static_cast<int>(heroes.size()) + livingMonsters;
            float scale = (fieldSize.x - 60.0f) / (figures * 250.0f);
            scale = std::min(scale, (fieldSize.y - 10.0f) / 360.0f);
            scale = std::max(0.4f, std::min(1.0f, scale));

            for (std::size_t i = 0; i < heroes.size(); ++i) {
                const json& hero = heroes[i];
                if (i > 0) ImGui::SameLine();
                std::string label = strOr(hero, "name");
                if (strOr(hero, "id") == myId) label += " (you)";
                if (strOr(hero, "id") == strOr(state, "active")) label += " *";
                if (!boolOr(hero, "connected", true)) label += " [left]";
                const bool canTarget = myTurn && ((iAmDm && (selectedMonster >= 0 || bossOrder)) ||
                                                  (!iAmDm && selectedCard >= 0 && targetsHero(selectedEffect)));
                ImGui::PushID(static_cast<int>(i));
                const bool clicked = drawCharacter({label, intOr(hero, "hp"), intOr(hero, "maxHp", 1), intOr(hero, "mana"),
                                                    intOr(hero, "maxMana"), textureOr0(heroTextures, strOr(hero, "heroId"))},
                                                   scale, canTarget);
                ImGui::PopID();
                if (clicked && canTarget) {
                    if (iAmDm && bossOrder) {
                        client.send({{"t", net::msg::BOSS}, {"hero", i}});
                        pendingOrder = "Boss -> " + strOr(hero, "name");
                    } else if (iAmDm) {
                        client.send({{"t", net::msg::COMMAND}, {"card", selectedCard}, {"monster", selectedMonster}, {"hero", i}});
                        pendingOrder = strOr(monsters[selectedMonster], "name") + " -> " + strOr(hero, "name");
                    } else {
                        client.send({{"t", net::msg::PLAY}, {"card", selectedCard}, {"target", i}, {"targetType", "hero"}});
                    }
                    clearSelection();
                    notice.clear();
                }
            }
            ImGui::SameLine(0, 40);
            int drawn = 0;
            for (std::size_t i = 0; i < monsters.size(); ++i) {
                const json& monster = monsters[i];
                if (!boolOr(monster, "alive")) continue;
                if (drawn++ > 0) ImGui::SameLine();
                const bool isBoss = boolOr(monster, "boss");
                const bool canTarget = myTurn && selectedCard >= 0 &&
                                       ((iAmDm && selectedMonster < 0 && !isBoss) || (!iAmDm && !targetsHero(selectedEffect)));
                ImGui::PushID(1000 + static_cast<int>(i));
                const bool clicked = drawCharacter({strOr(monster, "name") + (isBoss ? " (BOSS)" : ""), intOr(monster, "hp"),
                                                    intOr(monster, "maxHp", 1), 0, 0, textureOr0(monsterTextures, strOr(monster, "name"))},
                                                   scale, canTarget || (iAmDm && static_cast<int>(i) == selectedMonster));
                ImGui::PopID();
                if (clicked && canTarget) {
                    if (iAmDm) {
                        selectedMonster = static_cast<int>(i); // next click: the hero to attack
                    } else {
                        client.send({{"t", net::msg::PLAY}, {"card", selectedCard}, {"target", i}, {"targetType", "monster"}});
                        clearSelection();
                    }
                    notice.clear();
                }
            }
            ImGui::EndChild();

            // ---- My hand (the server only sends me my own cards)
            ImGui::BeginChild("Hand", ImVec2(0, 0), true);
            int myMana = iAmDm ? intOr(dm, "mana") : 0;
            for (const auto& hero : heroes)
                if (strOr(hero, "id") == myId) myMana = intOr(hero, "mana");
            if (hand.empty()) ImGui::TextDisabled(iAmDm && !myTurn ? "Your cards appear on your turn." : "No cards in hand.");
            const float rowY = ImGui::GetCursorPosY() + 15.0f;
            for (std::size_t i = 0; i < hand.size(); ++i) {
                const json& card = hand[i];
                const std::string effect = strOr(card, "effect");
                GLuint art = textureOr0(cardTextures, strOr(card, "name"));
                CardView view{strOr(card, "name"), intOr(card, "cost"), effectFromName(effect), intOr(card, "value"),
                              art ? art : emptyCardPicture,
                              effect == "DM_ATTACK" ? "Monster deals " + std::to_string(intOr(card, "value")) : ""};
                if (i > 0) ImGui::SameLine();
                const bool isSelected = static_cast<int>(i) == selectedCard;
                ImGui::SetCursorPosY(rowY - (isSelected ? 15.0f : 0.0f));
                ImGui::PushID(static_cast<int>(i));
                const bool clicked = drawCard(view, myTurn && !finished && myMana >= view.manaCost, isSelected);
                ImGui::PopID();
                if (clicked && myTurn) {
                    const bool deselect = isSelected;
                    clearSelection();
                    if (!deselect) selectedCard = static_cast<int>(i);
                    notice.clear();
                }
            }
            ImGui::EndChild();
            ImGui::End();

            // ---- Game over: Heroes_Win / Heroes_Lose
            if (finished) {
                const bool heroesWon = strOr(state, "winner") == "HEROES";
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::Begin("GameOver", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
                const GLuint picture = heroesWon ? heroesWinPicture : heroesLosePicture;
                if (picture) ImGui::Image((ImTextureID)(intptr_t)picture, ImVec2(480, 300));
                ImGui::SetWindowFontScale(1.6f);
                const bool iWon = heroesWon != iAmDm;
                ImGui::Text("%s win! %s", heroesWon ? "The heroes" : "The Dungeon Master", iWon ? "You won." : "You lost.");
                ImGui::SetWindowFontScale(1.0f);
                if (ImGui::Button("Back to menu", ImVec2(-1, 36))) backToMenu("");
                ImGui::End();
            }
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
