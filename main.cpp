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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Audio.h"

#include "GameData.h"
#include "NetClient.h"
#include "Protocol.h"
#include "Renderer.h"
#include "RoomServer.h"
#include "MatchResultStore.h"

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
        case LobbyRole::Wizard:  return "wizard";
        case LobbyRole::DM:      return net::DM_ROLE;
        default:                 return "";
    }
}

LobbyRole roleFromId(const std::string& id) {
    if (id == "warrior") return LobbyRole::Warrior;
    if (id == "healer") return LobbyRole::Healer;
    if (id == "wizard") return LobbyRole::Wizard;
    if (id == net::DM_ROLE) return LobbyRole::DM;
    return LobbyRole::None;
}
// Voice lines played when a role card is clicked (AI voices, created by elevenlabs.io).
const char* roleVoiceFile(LobbyRole role) {
    switch (role) {
        case LobbyRole::Warrior: return "assets/sounds/Warrior.mp3";
        case LobbyRole::Healer:  return "assets/sounds/Healer.mp3";
        case LobbyRole::Wizard:  return "assets/sounds/Wizard.mp3";       // not recorded yet: plays nothing until the file exists
        case LobbyRole::DM:      return "assets/sounds/DM.mp3";
        default:                 return nullptr;
    }
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
bool targetsHero(const std::string& effect) { return effect == "HEAL" || effect == "SHIELD" || effect == "BUFF"; }

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

void beginFullscreenWindow(const char* name, float bgAlpha = 1.0f) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowBgAlpha(bgAlpha);
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

// Pop up windows: help in lobby, and about us in start. 
void drawHelpPopup() {
    ImGui::SetNextWindowSize(ImVec2(580, 0));
    if (ImGui::BeginPopupModal("How to play", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Hosting & Connecting");
        ImGui::BulletText("This computer: runs the server inside the game. Friends on the SAME\n"
                          "network join with the address shown in your lobby.");
        ImGui::BulletText("A server (different networks): connect to a running dungeon_server\n"
                          "(e.g. on a cloud VM, or forwarded via playit.gg / Tailscale).");
        ImGui::BulletText("You can enter host:port (for example myserver.ply.gg:12345)\n"
                          "directly in the Server address box.");
        ImGui::BulletText("If your connection drops mid-match, the game automatically\n"
                          "reconnects you to your seat.");
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
void drawAboutPopup() {
    ImGui::SetNextWindowSize(ImVec2(600, 0));
    if (ImGui::BeginPopupModal("Who are we?", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("We are a team of seven Computer Science students at Technische Hochschule Ulm (THU). "
                   "We built Dungeon Party as our Software Project in the Winter Semester 2026/27, "
                   "working with OpenUP and Scrum.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Team");
        
        ImGui::BulletText("Suat Keles - Artwork");
        ImGui::BulletText("Irma Mejric - Product Owner");
        ImGui::BulletText("Hülya Bal - Game Data & Testing");
        ImGui::BulletText("Akshat Srivastava - Scrum Master");
        ImGui::BulletText("Taegan Rosin - Software Architect");
        ImGui::BulletText("Giray Kagan Angin - Backend Developer");
        ImGui::BulletText("Soroush Sadeghi - Frontend / UI Developer");

        ImGui::Spacing();
        ImGui::TextUnformatted("Supervisor: Prof. Dr. Klaus Baer");
        ImGui::TextUnformatted("Voices generated with elevenlabs.io");
        ImGui::Spacing();
        ImGui::TextWrapped("Dungeon Party: 3-6 heroes against one Dungeon Master, played over the network.");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
// Wood-and-gold look shared by the menu screens (Start, Lobby).
void pushMenuStyle() {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.13f, 0.08f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.21f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.29f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0.85f, 0.65f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 0.92f, 0.75f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.07f, 0.05f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
}

void popMenuStyle() {
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(6);
}
// Width and height of a texture that is already on the GPU, in pixels.
ImVec2 textureSize(GLuint texture) {
    GLint width = 0, height = 0;
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    return ImVec2(static_cast<float>(width), static_cast<float>(height));
}

// Biggest size of an image that fits inside a box without stretching it.
ImVec2 fitInside(ImVec2 image, ImVec2 box) {
    if (image.x <= 0 || image.y <= 0) return box;
    const float scaleX = box.x / image.x;
    const float scaleY = box.y / image.y;
    const float scale = scaleX < scaleY ? scaleX : scaleY;
    return ImVec2(image.x * scale, image.y * scale);
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
    initAudio(); // audio engine started
    // --- Loading new fonts
    ImGuiIO& fontIo = ImGui::GetIO();
    fontIo.Fonts->AddFontDefault();   // keep the normal font for all other text
    const std::string titleFontPath = projectPath("assets/fonts/Cinzel-Bold.ttf");
    ImFont* titleFont = std::filesystem::exists(titleFontPath)
        ? fontIo.Fonts->AddFontFromFileTTF(titleFontPath.c_str(), 48.0f)
        : nullptr; // missing font: titles fall back to the default font instead of crashing

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
    const GLuint battleBackground = loadTexture("assets/Level_1_Artwork.png"); // levels without a "background"
    std::map<int, GLuint> levelBackgrounds;  // level number -> its "background" from the level file
    if (gameData) {
        for (int n = 1;; ++n) {
            try {
                const LevelDefinition levelDef = gameData->loadLevel(n);
                if (GLuint tex = loadTexture(levelDef.background)) levelBackgrounds[n] = tex;
            } catch (const std::exception&) {
                break;
            }
        }
    }
    const GLuint menuBackground = loadTexture("assets/Menu_Background_AI_Generated.png");
    const GLuint startBackground = loadTexture("assets/Start_Background_AI_Generated.jpg");
    const GLuint lobbyBackground = loadTexture("assets/Lobby_Background_AI_Generated.jpg");
    const GLuint helpButton = loadTexture("assets/Help_Button.png");
    const GLuint heroesWinPicture = loadTexture("assets/Heroes_Win.png");
    const GLuint heroesLosePicture = loadTexture("assets/Heroes_Lose.png");
    const GLuint emptyCardPicture = loadTexture("assets/Empty_Card_Design.png"); // cards without their own art
    const GLuint wizardPicture = loadTexture("assets/Wizard_Artwork_AI_Generated.png"); // TEMPORARY until Suat's final art

    auto textureOr0 = [](const std::map<std::string, GLuint>& textures, const std::string& key) {
        auto it = textures.find(key);
        return it == textures.end() ? GLuint(0) : it->second;
    };
    auto roleTexture = [&](LobbyRole role) -> GLuint {
        if (role == LobbyRole::Warrior) return textureOr0(heroTextures, "warrior");
        if (role == LobbyRole::Healer) return textureOr0(heroTextures, "healer");
        if (role == LobbyRole::Wizard) {
            const GLuint finalArt = textureOr0(heroTextures, "wizard"); // heroes.json: assets/Wizard_Artwork.png
            return finalArt ? finalArt : wizardPicture;
        }
        if (role == LobbyRole::DM) return textureOr0(monsterTextures, "Final Boss");
        return 0;
    };

    // ---- Network state
    struct ReconnectSession {
        std::string address;
        std::uint16_t port = net::DEFAULT_PORT;
        std::string roomCode;
        std::string token;
        std::string playerName;
        bool valid() const { return !address.empty() && !roomCode.empty() && !token.empty(); }
        void clear() { address.clear(); port = net::DEFAULT_PORT; roomCode.clear(); token.clear(); playerName.clear(); }
    };

    const std::filesystem::path sessionFile = std::filesystem::temp_directory_path() / "dungeon_party_session.json";
    auto saveSessionToDisk = [&](const ReconnectSession& s) {
        if (!s.valid()) return;
        std::ofstream out(sessionFile, std::ios::trunc);
        if (!out) return;
        out << json{{"address", s.address}, {"port", s.port}, {"code", s.roomCode},
                    {"token", s.token}, {"name", s.playerName}}.dump();
    };
    auto clearSessionFromDisk = [&]() {
        std::error_code ec;
        std::filesystem::remove(sessionFile, ec);
    };
    auto loadSessionFromDisk = [&]() -> ReconnectSession {
        ReconnectSession s;
        std::ifstream in(sessionFile);
        if (!in) return s;
        json data = json::parse(in, nullptr, false);
        if (!data.is_object()) return s;
        s.address = strOr(data, "address");
        s.port = static_cast<std::uint16_t>(std::clamp(intOr(data, "port", net::DEFAULT_PORT), 1, 65535));
        s.roomCode = strOr(data, "code");
        s.token = strOr(data, "token");
        s.playerName = strOr(data, "name");
        return s;
    };

    std::unique_ptr<RoomServer> localServer; // only created when this player hosts on this computer
    NetClient client;
    OnlineScreen screen = OnlineScreen::Start;
    json pendingRequest;  // "create", "join", or "rejoin", sent as soon as the connection is up
    std::string notice;   // last error / info line shown to the player
    std::string roomCode, myId, hostId;
    json lobbyPlayers = json::array();
    std::string lobbyLevelName; // SCRUM-63: level chosen by the host, sent by the server
    json state;           // latest match state from the server
    const std::string localAddress = client.localNetworkAddress();

    ReconnectSession activeSession; // current room (kept on unexpected drop for auto-reconnect)
    ReconnectSession savedSession = loadSessionFromDisk(); // recovered if the app restarted mid-match
    bool reconnecting = false;
    int reconnectAttempts = 0;
    constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    bool rejoinSentThisAttempt = false;
    std::chrono::steady_clock::time_point nextReconnectAttempt{};

    std::string defaultServer;
    if (const char* envServer = std::getenv("DUNGEON_PARTY_SERVER")) defaultServer = net::trimWhitespace(envServer);
#ifdef DUNGEON_PARTY_DEFAULT_SERVER
    if (defaultServer.empty()) defaultServer = net::trimWhitespace(DUNGEON_PARTY_DEFAULT_SERVER);
#endif

    char nameBuf[net::MAX_NAME_LENGTH + 1] = "Player"; // the account's username

    // ---- Login (each player has an account with a permanent player ID on the server)
    char passBuf[65] = "";        // kept in memory only, never written to disk
    bool newAccount = false;      // tick "Create a new account" for the first visit to a server
    bool authSent = false;        // login/register sent on the current connection
    bool loggedIn = false;        // server accepted it on the current connection
    std::string myPlayerId;       // "P-K7RM2Q9X", from the server
    auto authMessage = [&]() {
        return json{{"t", newAccount ? net::msg::REGISTER : net::msg::LOGIN},
                    {"username", nameBuf}, {"password", passBuf}};
    };
    if (!savedSession.playerName.empty()) {
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", savedSession.playerName.c_str());
    }
    char serverBuf[128] = "";       // address to JOIN (supports host or host:port)
    char hostServerBuf[128] = "";   // address of a dedicated server to HOST on
    if (!defaultServer.empty()) {
        std::snprintf(serverBuf, sizeof(serverBuf), "%s", defaultServer.c_str());
        std::snprintf(hostServerBuf, sizeof(hostServerBuf), "%s", defaultServer.c_str());
    }
    int hostWhere = defaultServer.empty() ? 0 : 1; // 0 = this computer, 1 = a server

    // SCRUM-63: levels the host can pick. Filled once by loading level_1, level_2, ...
    // until a file is missing, so new level files show up without code changes.
    std::vector<std::pair<int, std::string>> levelChoices; // {number, name}
    if (gameData) {
        for (int n = 1;; ++n) {
            try {
                levelChoices.push_back({n, gameData->loadLevel(n).name});
            } catch (const std::exception&) {
                break;
            }
        }
    }
    int levelIndex = 0; // index into levelChoices
    int port = net::DEFAULT_PORT;
    char codeBuf[16] = "";

    // ---- Battle selection: what the player has clicked so far
    int selectedCard = -1;     // hand index
    int selectedMonster = -1;  // DM: the monster that will attack
    bool bossOrder = false;    // DM: choosing a target for the final boss
    std::string lastActive;    // whose turn it was in the last state (to notice a new turn)

    // Turn timer display. The SERVER ends turns; the client only counts down from what it was told.
    double turnEndsAt = 0.0;           // glfwGetTime() when the current turn runs out (0 = no timer)
    int turnSecondsTotal = 0;
    std::string infoText;              // e.g. "Kagan ran out of time" - survives the turn change below
    double infoUntil = 0.0;

    auto clearSelection = [&] {
        selectedCard = -1;
        selectedMonster = -1;
        bossOrder = false;
    };

    auto backToMenu = [&](const std::string& why, bool keepSavedSession = false) {
        client.disconnect();
        screen = OnlineScreen::Menu;
        notice = why;
        roomCode.clear(); myId.clear(); hostId.clear();
        state = nullptr;
        pendingRequest = nullptr;
        authSent = false; loggedIn = false;
        reconnecting = false;
        reconnectAttempts = 0;
        rejoinSentThisAttempt = false;
        if (!keepSavedSession) {
            savedSession.clear();
            clearSessionFromDisk();
        }
        activeSession.clear();
        clearSelection();
        lastActive.clear();
        turnEndsAt = 0.0; turnSecondsTotal = 0; infoText.clear();
    };

    auto leaveAndBackToMenu = [&](const std::string& why) {
        if (client.status() == NetClient::Status::Connected) {
            client.send({{"t", net::msg::LEAVE}});
        }
        backToMenu(why, false);
    };

    auto startConnecting = [&](const std::string& rawAddress, json request) {
        notice.clear();
        const auto fallbackPort = static_cast<std::uint16_t>(std::clamp(port, 1, 65535));
        net::ServerEndpoint endpoint = net::parseServerEndpoint(rawAddress, fallbackPort);
        const std::string resolvedHost = isThisComputer(endpoint.host) ? "127.0.0.1" : endpoint.host;
        if (!client.connect(resolvedHost, endpoint.port)) {
            notice = client.lastError();
            return;
        }
        activeSession.clear();
        activeSession.address = resolvedHost;
        activeSession.port = endpoint.port;
        activeSession.playerName = nameBuf;
        reconnecting = false;
        reconnectAttempts = 0;
        rejoinSentThisAttempt = false;
        authSent = false; loggedIn = false; // every new connection logs in again
        pendingRequest = std::move(request);
        screen = OnlineScreen::Connecting;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ---- 1. Run the embedded server if this computer is hosting
        if (localServer) localServer->poll(0);

        // ---- 2. Network client: once per frame, never blocks
        client.poll();
        // Log in first on every connection (also after an automatic reconnect).
        // The actual request (create / join / rejoin) is sent when the server answers "auth".
        if (client.status() == NetClient::Status::Connected && !authSent &&
            (screen == OnlineScreen::Connecting || reconnecting)) {
            client.send(authMessage());
            authSent = true;
        }

        if (reconnecting) {
            const auto now = std::chrono::steady_clock::now();
            if (client.status() == NetClient::Status::Connected && loggedIn && !rejoinSentThisAttempt) {
                if (state.is_object()) {
                    // match already started, reclaim our seat with the token
                    client.send({{"t", net::msg::REJOIN}, {"code", activeSession.roomCode}, {"token", activeSession.token}});
                } else {
                    // still in the lobby, join the room again by code
                    client.send({{"t", net::msg::JOIN}, {"name", activeSession.playerName}, {"code", activeSession.roomCode}});
                }
                rejoinSentThisAttempt = true;
            } else if (client.status() == NetClient::Status::Disconnected && now >= nextReconnectAttempt) {
                if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
                    savedSession = activeSession;
                    backToMenu("Lost connection. Click Reconnect to try again.", true);
                } else {
                    ++reconnectAttempts;
                    rejoinSentThisAttempt = false;
                    authSent = false; loggedIn = false;
                    nextReconnectAttempt = now + std::chrono::seconds(2);
                    client.connect(activeSession.address, activeSession.port);
                }
            }
        }

        bool closedByServer = false;
        for (const json& message : client.takeMessages()) {
            const std::string type = strOr(message, "t");
            if (type == net::msg::AUTH) {
                loggedIn = true;
                myPlayerId = strOr(message, "playerId");
                std::snprintf(nameBuf, sizeof(nameBuf), "%s", strOr(message, "username", nameBuf).c_str());
                newAccount = false; // the account exists now; reconnects log in normally
                if (screen == OnlineScreen::Connecting && !pendingRequest.is_null()) {
                    client.send(pendingRequest);
                    pendingRequest = nullptr;
                }
            } else if (type == net::msg::JOINED) {
                roomCode = strOr(message, "code");
                myId = strOr(message, "you");
                activeSession.roomCode = roomCode;
                activeSession.token = strOr(message, "token", activeSession.token);
                if (!localServer) saveSessionToDisk(activeSession);
                savedSession.clear();
                reconnecting = false;
                reconnectAttempts = 0;
                rejoinSentThisAttempt = false;
                if (screen == OnlineScreen::Connecting) screen = OnlineScreen::Lobby;
                notice.clear();
            } else if (type == net::msg::LOBBY) {
                roomCode = strOr(message, "code", roomCode);
                hostId = strOr(message, "host");
                lobbyPlayers = message.value("players", json::array());
                lobbyLevelName = strOr(message, "levelName");
                screen = OnlineScreen::Lobby;
            } else if (type == net::msg::STATE) {
                state = message;
                turnSecondsTotal = intOr(message, "turnSeconds");
                // Use our own clock plus the time left the server reported: no clock syncing needed.
                turnEndsAt = turnSecondsTotal > 0 ? glfwGetTime() + intOr(message, "turnMsLeft") / 1000.0 : 0.0;
                hostId = strOr(message, "host", hostId);
                screen = OnlineScreen::Battle;
                if (!state["winner"].is_null()) clearSessionFromDisk(); // finished match needs no reconnect
                const std::string active = message["active"].is_string() ? message["active"].get<std::string>() : "";
                if (active != lastActive) { // a new turn started: forget the old selection
                    lastActive = active;
                    clearSelection();
                    notice.clear(); // an old hint/error must not stay on screen into the next turn (SCRUM-40)
                }
            } else if (type == net::msg::INFO) {
                infoText = strOr(message, "msg");
                infoUntil = glfwGetTime() + 4.0; // show it for 4 seconds
            } else if (type == net::msg::ERROR) {
                notice = strOr(message, "msg");
                if (reconnecting) {
                    // room gone or token rejected, stop retrying
                    closedByServer = true;
                    backToMenu(notice, false);
                } else if (screen == OnlineScreen::Connecting) {
                    closedByServer = true;
                    backToMenu(notice, false);
                }
            } else if (type == net::msg::CLOSED) {
                closedByServer = true;
                backToMenu(strOr(message, "msg", "The room was closed"), false);
            }
        }

        if (!closedByServer && !reconnecting && screen != OnlineScreen::Start && screen != OnlineScreen::Menu &&
            client.status() == NetClient::Status::Disconnected) {
            if ((screen == OnlineScreen::Lobby || screen == OnlineScreen::Battle) && activeSession.valid()) {
                reconnecting = true;
                reconnectAttempts = 0;
                rejoinSentThisAttempt = false;
                nextReconnectAttempt = std::chrono::steady_clock::now();
            } else {
                backToMenu(client.lastError().empty() ? "Disconnected" : client.lastError(), false);
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
            drawBackground(startBackground);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.25f, io.DisplaySize.y * 0.40f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 0));
            ImGui::Begin("Start", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoBackground);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PushFont(titleFont);
            const char* title = "DUNGEON PARTY";
            float titleWidth = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - titleWidth) * 0.5f);

            ImVec2 titlePos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(titlePos.x + 3, titlePos.y + 3), IM_COL32(0, 0, 0, 200), title);
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.45f, 1.0f), "%s", title);
            ImGui::PopFont();

            ImGui::Dummy(ImVec2(0, 12));
            ImGui::SetWindowFontScale(1.5f);
            if (!dataErrorMessage.empty()) {
                ImGui::TextWrapped("%s", dataErrorMessage.c_str());
            } 
            else
            {
                pushMenuStyle();
                if(ImGui::Button("Play", ImVec2(-1, 50))) {
                    screen = OnlineScreen::Menu;
                }
                ImGui::Dummy(ImVec2(0, 12));
                if(ImGui::Button("About us", ImVec2(-1,50))){
                    ImGui::OpenPopup("Who are we?");
                }
                ImGui::SetWindowFontScale(1.0f);
                drawAboutPopup();
                popMenuStyle();
            } 
            ImGui::End();
        }
        // ================= SCREEN 2: Host / Join =================
        // SCRUM-32 layout (Soroush): name on top, Host in the left parchment scroll, Join in the right one.
        else if (screen == OnlineScreen::Menu || screen == OnlineScreen::Connecting) {
            drawBackground(menuBackground);
            const ImVec2 win = io.DisplaySize;
            const ImGuiWindowFlags fixedFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

            if (screen == OnlineScreen::Connecting) {
                ImGui::SetNextWindowPos(ImVec2(win.x * 0.5f, win.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(420, 110));
                ImGui::Begin("Connecting", nullptr, fixedFlags);
                ImGui::Text("Connecting to %s:%u ...", activeSession.address.c_str(), activeSession.port);
                if (ImGui::Button("Cancel", ImVec2(-1, 0))) backToMenu("", false);
                ImGui::End();
            } else {
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(win);
                ImGui::Begin("Menu", nullptr, fixedFlags | ImGuiWindowFlags_NoBackground);

                if (ImGui::Button("Back")) {
                    notice.clear();
                    screen = OnlineScreen::Start;
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 52);
                const bool helpClicked = helpButton ? ImGui::ImageButton("help", (ImTextureID)(intptr_t)helpButton, ImVec2(28, 28))
                                                    : ImGui::Button("?", ImVec2(36, 36));
                if (helpClicked) ImGui::OpenPopup("How to play");
                drawHelpPopup();

                // Login fields, centred above the two scrolls. The account lives on the server you
                // host on or join; the first time there, tick "Create a new account".
                ImGui::PushStyleColor(ImGuiCol_FrameBg,   ImVec4(0.93f, 0.85f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,      ImVec4(0.25f, 0.15f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.45f, 0.25f, 0.05f, 1.0f));
                ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.14f));
                ImGui::SetNextItemWidth(win.x * 0.3f);
                ImGui::InputTextWithHint("##name", "Username", nameBuf, sizeof(nameBuf));
                ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.19f));
                ImGui::SetNextItemWidth(win.x * 0.3f);
                ImGui::InputTextWithHint("##password", "Password", passBuf, sizeof(passBuf), ImGuiInputTextFlags_Password);
                ImGui::PopStyleColor(2);
                ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.235f));
                ImGui::Checkbox("Create a new account", &newAccount);
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.7f, 0.85f), "(don't reuse a real password: traffic is not encrypted)");
                // Host and Join only work once username and password are filled in
                const bool nameFilled = nameBuf[0] != '\0' && passBuf[0] != '\0';

                if (!notice.empty()) {
                    ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.28f));
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", notice.c_str());
                }

                // The two panels sit inside the empty parchment scrolls of Menu_Background_AI_Generated.png
                const float panelW = win.x * 0.235f;
                const float panelH = win.y * 0.43f;
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.25f, 0.15f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.85f, 0.70f, 0.40f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.80f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.75f, 0.60f, 0.30f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.93f, 0.85f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark,     ImVec4(0.45f, 0.25f, 0.05f, 1.0f));

                // ---- Left scroll: Host
                ImGui::SetCursorPos(ImVec2(win.x * 0.07f, win.y * 0.35f));
                ImGui::BeginChild("Host", ImVec2(panelW, panelH), true, ImGuiWindowFlags_NoBackground);
                ImGui::TextUnformatted("HOST A PARTY");
                ImGui::TextWrapped("Create a room and share its code with your friends.");
                ImGui::RadioButton("This computer", &hostWhere, 0);
                ImGui::RadioButton("A server", &hostWhere, 1);
                if (hostWhere == 1) {
                    ImGui::SetNextItemWidth(-1);
                    ImGui::InputTextWithHint("##hostserver", "Server host[:port]", hostServerBuf, sizeof(hostServerBuf));
                } else {
                    ImGui::TextWrapped("Same network only unless forwarded (see ?).");
                }
                // SCRUM-63: level picker (only the host sees it)
                if (!levelChoices.empty()) {
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##level", levelChoices[levelIndex].second.c_str())) {
                        for (int i = 0; i < static_cast<int>(levelChoices.size()); ++i) {
                            if (ImGui::Selectable(levelChoices[i].second.c_str(), i == levelIndex)) levelIndex = i;
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::BeginDisabled(!nameFilled);
                if (ImGui::Button("New Adventure", ImVec2(-1, 44))) {
                    const int chosenLevel = levelChoices.empty() ? 1 : levelChoices[levelIndex].first;
                    const json create = {{"t", net::msg::CREATE}, {"name", nameBuf}, {"level", chosenLevel}};
                    if (hostWhere == 1 && !isThisComputer(net::trimWhitespace(hostServerBuf))) {
                        startConnecting(hostServerBuf, create); // a dedicated dungeon_server somewhere else
                    } else {
                        if (!localServer && gameData) {
                            try {
                                auto server = std::make_unique<RoomServer>(
                                    *gameData, static_cast<std::uint16_t>(std::clamp(port, 1, 65535)), 0,
                                    std::chrono::seconds(net::EMPTY_ROOM_GRACE_SECONDS),
                                    makeMatchResultSink(projectPath("match-results.sqlite")));
                                server->setAccountsFile(projectPath("accounts.json")); // ignored by git
                                localServer = std::move(server);
                            } catch (const std::exception& e) {
                                // Port already taken (e.g. another window on this computer is hosting):
                                // we can still create a room on that server.
                                std::cerr << "Local server note: " << e.what() << "\n";
                            }
                        }
                        startConnecting("127.0.0.1", create);
                    }
                }
                ImGui::EndDisabled();
                ImGui::EndChild();

                // ---- Right scroll: Join
                ImGui::SetCursorPos(ImVec2(win.x * 0.71f, win.y * 0.35f));
                ImGui::BeginChild("Join", ImVec2(panelW, panelH), true, ImGuiWindowFlags_NoBackground);
                ImGui::TextUnformatted("JOIN A PARTY");
                ImGui::TextWrapped("Enter the server address and room code:");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##joinserver", "Server host[:port]", serverBuf, sizeof(serverBuf));
                ImGui::SetNextItemWidth(-1);
                ImGui::InputInt("##port", &port);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##code", "Room code", codeBuf, sizeof(codeBuf), ImGuiInputTextFlags_CharsUppercase);
                ImGui::BeginDisabled(!nameFilled);
                if (ImGui::Button("Join", ImVec2(-1, 44))) {
                    startConnecting(serverBuf, {{"t", net::msg::JOIN}, {"name", nameBuf}, {"code", codeBuf}});
                }
                ImGui::EndDisabled();
                if (savedSession.valid()) {
                    ImGui::Spacing();
                    if (ImGui::Button(("Reconnect to " + savedSession.roomCode).c_str(), ImVec2(-1, 36))) {
                        const auto sessionCopy = savedSession;
                        startConnecting(sessionCopy.address + ":" + std::to_string(sessionCopy.port),
                                        {{"t", net::msg::REJOIN}, {"code", sessionCopy.roomCode}, {"token", sessionCopy.token}});
                        activeSession = sessionCopy;
                        savedSession = sessionCopy;
                    }
                }
                ImGui::EndChild();

                ImGui::PopStyleColor(6); // every pushed color must be popped
                ImGui::End();
            }
        }
        // ================= SCREEN 3: Lobby =================
        else if (screen == OnlineScreen::Lobby) {
            drawBackground(lobbyBackground);
            beginFullscreenWindow("Lobby", 0.0f);
            pushMenuStyle();
            ImGui::BeginChild("TopBar", ImVec2(0, 80), ImGuiChildFlags_Borders);
            ImGui::PushFont(titleFont);
            ImGui::SetWindowFontScale(0.6f);
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.45f, 1.0f), "Room %s", roomCode.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            ImGui::SameLine();
            if (ImGui::Button("Copy")) ImGui::SetClipboardText(roomCode.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 130);
            if (ImGui::Button("Leave", ImVec2(110, 36))) leaveAndBackToMenu("");
            if (localServer) {
                ImGui::TextColored(ImVec4(0.80f, 0.75f, 0.65f, 1.0f),
                                "Hosted on this computer. Same network: server address %s. Other networks: see ? in the menu.",
                                localAddress.empty() ? "(unknown - run ipconfig)" : localAddress.c_str());
            }
            ImGui::EndChild();
            popMenuStyle();

            // Players and the roles the SERVER has confirmed
            LobbyRole myRole = LobbyRole::None;
            std::string dmTakenBy;
            const ImVec4 gold(1.0f, 0.82f, 0.45f, 1.0f);
            const ImVec4 cream(1.0f, 0.92f, 0.75f, 1.0f);
            const float panelHeight = 0;  // shared-height
            pushMenuStyle();
            ImGui::BeginChild("Party", ImVec2(320, panelHeight), ImGuiChildFlags_Borders);
            ImGui::PushFont(titleFont);
            ImGui::SetWindowFontScale(0.5f);
            ImGui::TextColored(gold, "Party %zu/%zu", lobbyPlayers.size(), net::MAX_PLAYERS_PER_ROOM);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            ImGui::Separator();
            if (!lobbyLevelName.empty()) ImGui::TextColored(cream, "Level: %s", lobbyLevelName.c_str()); // SCRUM-63
            ImGui::Spacing();
            for (const auto& p : lobbyPlayers) {
                const std::string id = strOr(p, "id");
                const LobbyRole role = roleFromId(strOr(p, "role"));
                if (id == myId) myRole = role;
                if (role == LobbyRole::DM && id != myId) dmTakenBy = strOr(p, "name");
                const ImVec4 color = (id == myId) ? gold : cream;
                ImGui::TextColored(color, "%s%s", strOr(p, "name").c_str(), id == hostId ? " (host)" : "");
                ImGui::SameLine(190);
                ImGui::TextColored(color, "%s", roleName(role));
                // Permanent account ID (login), small and dimmed under the name
                if (!strOr(p, "playerId").empty()) ImGui::TextDisabled("  %s", strOr(p, "playerId").c_str());
            }
            ImGui::EndChild();
            popMenuStyle();
            ImGui::SameLine();
            pushMenuStyle();
            ImGui::BeginChild("Roles", ImVec2(0, panelHeight), ImGuiChildFlags_Borders);
            popMenuStyle();

            // ROLE CARDS (SCRUM-35, Soroush): clicking sends the role to the server.
        
            ImGui::PushFont(titleFont);
            ImGui::SetWindowFontScale(0.5f);
            ImGui::TextColored(gold, "Choose your role");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            ImGui::Spacing();

            pushMenuStyle();
            const ImVec2 cardSize(180, 260);
            const LobbyRole roles[4] = {LobbyRole::Warrior, LobbyRole::Healer, LobbyRole::Wizard, LobbyRole::DM};
            for (LobbyRole role : roles) {
                const bool locked = role == LobbyRole::DM && !dmTakenBy.empty(); // every hero is playable now
                const bool chosen = role == myRole;

                ImGui::BeginGroup();
                ImGui::BeginDisabled(locked);
                if (chosen) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.29f, 0.14f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.29f, 0.14f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(1.0f, 0.85f, 0.35f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 4.0f);
                }
                ImGui::PushID(roleName(role));
                const bool clicked = ImGui::Button("##card", cardSize);
                ImGui::PopID();
              

                if (const GLuint tex = roleTexture(role)) {
                    const ImVec2 cardMin = ImGui::GetItemRectMin();
                    const ImVec2 picture = fitInside(textureSize(tex), ImVec2(cardSize.x - 16, cardSize.y - 16));
                    const ImVec2 p0(cardMin.x + (cardSize.x - picture.x) * 0.5f, cardMin.y + (cardSize.y - picture.y) * 0.5f);
                    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tex, p0, ImVec2(p0.x + picture.x, p0.y + picture.y),
                                                        ImVec2(0, 0), ImVec2(1, 1), locked ? IM_COL32(255, 255, 255, 90) : IM_COL32_WHITE);
                }
                if (chosen) {
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);
                }
                ImGui::EndDisabled();

                if (chosen) {
                    ImGui::PushFont(titleFont);
                    ImGui::SetWindowFontScale(0.4f);
                }
                const float nameWidth = ImGui::CalcTextSize(roleName(role)).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cardSize.x - nameWidth) * 0.5f);
                ImGui::TextColored(chosen ? gold : cream, "%s", roleName(role));
                if (chosen) {
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::PopFont();
                }
                std::string status;
                if (role == LobbyRole::DM && !dmTakenBy.empty()) status = "taken by " + dmTakenBy;
                if (!status.empty()) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cardSize.x - ImGui::CalcTextSize(status.c_str()).x) * 0.5f);
                    ImGui::TextDisabled("%s", status.c_str());
                }
                ImGui::EndGroup();

                if (clicked && !chosen) {
                    client.send({{"t", net::msg::ROLE}, {"role", roleId(role)}});
                    if (const char* file = roleVoiceFile(role)) playVoice(projectPath(file));
                }
                ImGui::SameLine(0, 24);
            }
            ImGui::NewLine();
            popMenuStyle();
            ImGui::Spacing();
            pushMenuStyle();
            const ImVec4 muted(0.80f, 0.75f, 0.65f, 1.0f);
            if (myId == hostId) {
                ImGui::SetWindowFontScale(1.3f);
                if (ImGui::Button("Start match", ImVec2(220, 48))) client.send({{"t", net::msg::START}});
                ImGui::SetWindowFontScale(1.0f);
                ImGui::TextColored(muted, "No DM chosen = the computer plays the DM.");
            } else {
                ImGui::TextColored(cream, "Waiting for the host to start...");
            }
            popMenuStyle(); 
            ImGui::EndChild();
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

            // ---- Turn countdown (everyone sees it; the server ends the turn at 0)
            if (turnEndsAt > 0.0 && !finished) {
                const double left = std::max(0.0, turnEndsAt - glfwGetTime());
                const int secondsLeft = static_cast<int>(std::ceil(left));
                char clock[16];
                std::snprintf(clock, sizeof(clock), "%d:%02d", secondsLeft / 60, secondsLeft % 60);
                const ImVec4 colour = secondsLeft <= 10 ? ImVec4(0.90f, 0.20f, 0.15f, 1.0f)   // red: hurry
                                    : secondsLeft <= 20 ? ImVec4(0.95f, 0.60f, 0.10f, 1.0f)   // orange
                                                        : ImVec4(0.30f, 0.70f, 0.35f, 1.0f);  // green
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, colour);
                ImGui::ProgressBar(static_cast<float>(left / turnSecondsTotal), ImVec2(110, 0), clock);
                ImGui::PopStyleColor();
                if (myTurn && secondsLeft <= 10) {
                    ImGui::SameLine();
                    ImGui::TextColored(colour, "Hurry!");
                }
            }

            ImGui::SameLine();
            const bool dmOffline = boolOr(dm, "human", false) && !boolOr(dm, "connected", true);
            ImGui::TextDisabled("| DM: %s%s%s", strOr(dm, "name").c_str(), iAmDm ? " (you)" : "",
                                dmOffline ? " [offline]" : "");
            if (iAmDm && dm.contains("mana")) {
                ImGui::SameLine();
                ImGui::Text("Mana %d/%d | Deck %d", intOr(dm, "mana"), intOr(dm, "maxMana"), intOr(dm, "deckSize"));
            }
            ImGui::SameLine(io.DisplaySize.x - 70);
            if (ImGui::SmallButton("Leave")) leaveAndBackToMenu("");
            ImGui::EndChild();

            // ---- What to click next
            std::string selectedEffect = (selectedCard >= 0 && selectedCard < (int)hand.size()) ? strOr(hand[selectedCard], "effect") : "";
            std::string instruction;
            if (!myTurn) instruction = "Waiting for " + activeName + "...";
            else if (iAmDm) {
                if (bossOrder) instruction = "Click the hero the FINAL BOSS should attack.";
                else if (selectedCard < 0) instruction = "Pick a card (or Boss attack), then a monster, then the hero it targets. Orders happen on End turn.";
                else if (selectedMonster < 0) instruction = "Now click the monster that should attack.";
                else instruction = "Now click the hero " + strOr(monsters[selectedMonster], "name") + " should attack.";
            } else {
                if (selectedCard < 0) instruction = "Pick a card from your hand.";
                else if (targetsHero(selectedEffect)) instruction = "Click the hero to use it on.";
                else instruction = "Click a monster to attack.";
            }
            ImGui::TextUnformatted(instruction.c_str());
            // Swarm progress: "Swarm 1 of 2", then the Dungeon Master's boss
            if (boolOr(state, "bossArrived")) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.25f, 1.0f), "  |  The Dungeon Master is on the battlefield!");
            } else if (intOr(state, "swarms") > 0) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.45f, 1.0f), "  |  Swarm %d of %d - the Dungeon Master waits",
                                   intOr(state, "swarm"), intOr(state, "swarms"));
            }
            if (!infoText.empty() && glfwGetTime() < infoUntil) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "  %s", infoText.c_str());
            }
            if (iAmDm && myTurn && state.value("canBossAttack", false)) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Boss attack")) { clearSelection(); bossOrder = true; }
            }
            // DM view (SCRUM-40): the orders the server has queued this turn
            const json queued = dm.value("queued", json::array());
            if (iAmDm && !queued.empty()) {
                std::string list;
                for (const auto& order : queued)
                    list += (list.empty() ? "" : ", ") + strOr(order, "monster") + " -> " + strOr(order, "hero") +
                            " (" + strOr(order, "card") + ")";
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1), "Orders: %s", list.c_str());
            }
            // DM view (SCRUM-54): armed traps, only the DM's state contains them
            const json traps = dm.value("traps", json::array());
            if (iAmDm && !traps.empty()) {
                std::string list;
                for (const auto& trap : traps)
                    list += (list.empty() ? "" : ", ") + strOr(trap, "type") + " on " + strOr(trap, "hero");
                ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.95f, 1), "Traps: %s", list.c_str());
            }
            drawNotice(notice);

            // ---- Battlefield
            ImGui::BeginChild("Battlefield", ImVec2(0, ImGui::GetContentRegionAvail().y - 222), true, ImGuiWindowFlags_NoScrollbar);
            const ImVec2 fieldSize = ImGui::GetContentRegionAvail();
            // The background of the level being played (level file "background"), else the default.
            GLuint background = battleBackground;
            if (auto it = levelBackgrounds.find(intOr(state, "level", 1)); it != levelBackgrounds.end()) background = it->second;
            if (background) {
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)background, origin,
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
                if (!boolOr(hero, "connected", true)) label += " [offline]";
                const bool canTarget = myTurn && ((iAmDm && (selectedMonster >= 0 || bossOrder)) ||
                                                  (!iAmDm && selectedCard >= 0 && targetsHero(selectedEffect)));
                ImGui::PushID(static_cast<int>(i));
                const bool clicked = drawCharacter({label, intOr(hero, "hp"), intOr(hero, "maxHp", 1), intOr(hero, "mana"),
                                                    intOr(hero, "maxMana"), textureOr0(heroTextures, strOr(hero, "heroId")), intOr(hero, "buffTurns")},
                                                   scale, canTarget);
                ImGui::PopID();
                if (clicked && canTarget) {
                    if (iAmDm && bossOrder) {
                        client.send({{"t", net::msg::BOSS}, {"hero", i}});
                    } else if (iAmDm) {
                        client.send({{"t", net::msg::COMMAND}, {"card", selectedCard}, {"monster", selectedMonster}, {"hero", i}});
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
                              effect == "DM_ATTACK"   ? "Monster deals " + std::to_string(intOr(card, "value")) :
                              effect == "DM_WEAKNESS" ? "Weakens a hero by " + std::to_string(intOr(card, "value")) : ""};
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
                if (ImGui::Button("Back to menu", ImVec2(-1, 36))) leaveAndBackToMenu("");
                ImGui::End();
            }
        }

        if (reconnecting) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(420, 0));
            ImGui::Begin("Reconnecting", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(1.3f);
            ImGui::TextUnformatted("Connection lost");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextWrapped("Reconnecting to room %s on %s:%u (attempt %d/%d)...",
                               activeSession.roomCode.c_str(), activeSession.address.c_str(),
                               activeSession.port, reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
            if (ImGui::Button("Leave match", ImVec2(-1, 34))) leaveAndBackToMenu("");
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    client.disconnect();
    localServer.reset();
    shutdownAudio(); 
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
