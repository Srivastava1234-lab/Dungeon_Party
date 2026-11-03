#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include "Renderer.h"
#include "GameData.h"
#include "Match.h"
#include "Hero.h"
#include "HeroCard.h"
#include "Monster.h"
#include "DungeonMaster.h"
#include "TurnIndicator.h"

// Asset paths in the data files are relative to the project folder (e.g. "assets/Warrior_Artwork.png").
// CMake passes the absolute project folder in, so the game finds its files no matter which
// folder it is started from (VS Code, CLion and Visual Studio all start it in the build folder).
static std::string projectPath(const std::string& relative)
{
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

    default:
        return "No Role";
    }
}
int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Request an OpenGL 3.3 core profile since it works with no issues and I cant be asked to look at newer ones
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(800, 600, "WOOOOOOO DUNGEON MASTERS", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync: without it the loop runs at 100% GPU/CPU

    // Initialize GLAD (load OpenGL function pointers) BEFORE anything uses OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Create and connect ImGui to window
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into whatever folder we were started in
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::cout << "OpenGL loaded: " << glGetString(GL_VERSION) << "\n";

    // Test state and match setup
    Screen screen = Screen::Start;
    std::unique_ptr<Match> currentMatch = nullptr;

    // Initialize GameData loading from the data/ folder.
    // If a data file is missing or wrong we show the error in the window instead of crashing.
    std::unique_ptr<GameData> gameData;
    std::string errorMessage;
    try
    {
        gameData = std::make_unique<GameData>(GameData::defaultDataDirectory());
    }
    catch (const std::exception& e)
    {
        errorMessage = std::string("Could not load game data:\n") + e.what();
        std::cerr << errorMessage << "\n";
    }

    // Load every PNG once at startup. Heroes are keyed by hero id, monsters by name
    // (those are the values the Hero/Monster objects carry).
    std::map<std::string, GLuint> heroTextures;
    std::map<std::string, GLuint> monsterTextures;
    GLuint bgTexture = 0;
    GLuint menuTexture = 0;
    GLuint warriorTexture = 0;
    GLuint healerTexture = 0;
    GLuint wizardTexture = 0;
    GLuint dmTexture = 0;
    
    int width = 0, height = 0;
    if (gameData)
    {
        for (const auto& heroDef : gameData->getHeroes())
        {
            GLuint tex = 0;
            if (!heroDef.image.empty() && LoadTextureFromFile(projectPath(heroDef.image).c_str(), &tex, &width, &height))
                heroTextures[heroDef.id] = tex;
        }
        for (const auto& monsterDef : gameData->getMonsters())
        {
            GLuint tex = 0;
            if (!monsterDef.image.empty() && LoadTextureFromFile(projectPath(monsterDef.image).c_str(), &tex, &width, &height))
                monsterTextures[monsterDef.name] = tex;
        }
    }
    LoadTextureFromFile(projectPath("assets/Level_1_Artwork.png").c_str(), &bgTexture, &width, &height);
    LoadTextureFromFile(projectPath("assets/Menu_Background_AI_Generated.png").c_str(), &menuTexture, &width, &height);
    LoadTextureFromFile(projectPath("assets/Warrior_Artwork.png").c_str(), &warriorTexture, &width, &height);
    LoadTextureFromFile(projectPath("assets/Healer_Artwork.png").c_str(), &healerTexture, &width, &height);

    auto roleTexture = [&](LobbyRole role) -> GLuint {
        switch(role){

            case LobbyRole::Warrior:
                return warriorTexture;

            case LobbyRole::Healer:
                return healerTexture;

            case LobbyRole::Wizard:
                return wizardTexture;
            
            case LobbyRole::DM:
                return dmTexture;

            default:
                return 0;
            }

    };

    auto textureOr0 = [](const std::map<std::string, GLuint>& textures, const std::string& key) -> GLuint {
        auto it = textures.find(key);
        return it == textures.end() ? 0 : it->second;
    };


    int selectedCard = -1;   // nothing is selected
    char nameBuf[17] = "";   // player name, up to 16 characters
    char codeBuf[7] = "";    // room code, 6 characters
    bool showHelp = false;   
    bool isHost = false;     // Only host can start the match
    LobbyPlayer cuurentPlayer = { nameBuf, LobbyRole::None};
    

    // FAKE DATA: until  server can send the code
    std::string roomCode = "PLAYER";
    std::vector<LobbyPlayer> lobbyPlayers = {{"Player2", LobbyRole::Healer}, {"Player 3", LobbyRole::DM}};
    // Ends here


    // *****************************************************
    // Main loop clear screen to a color until window closed
    // *****************************************************
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.12f, 0.34f, 0.56f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Start match button
        if (screen == Screen::Start)
        {
            ImGuiIO &io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 220));

            ImGui::Begin("Start", nullptr,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::Text("DUNGEON PARTY");
            if (!errorMessage.empty())
            {
                ImGui::SetWindowFontScale(1.0f);
                ImGui::TextWrapped("%s", errorMessage.c_str());
            }
            else if (ImGui::Button("Menu", ImVec2(-1, 70)))
            {
                screen = Screen::Menu;
            }
            ImGui::End();
        }

         else if (screen == Screen::Menu)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Menu", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            if (menuTexture != 0)
            {
                ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)menuTexture,
                                                    ImVec2(0, 0), ImGui::GetIO().DisplaySize);
            }

            if(ImGui::Button("Back")){
                screen = Screen::Start;
            }
            ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                if (ImGui::Button("?"))
                {
                    showHelp = !showHelp;
                }
            ImVec2 win = ImGui::GetIO().DisplaySize;
            ImGui::SetCursorPos(ImVec2(win.x * 0.35f, win.y * 0.2f));
            ImGui::SetNextItemWidth(win.x * 0.3f);

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.93f, 0.85f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.25f, 0.15f, 0.05f, 1.0f));
            ImGui::InputTextWithHint("##name", "Your name", nameBuf, sizeof(nameBuf));
            ImGui::PopStyleColor(2);
            // A player can enter or create a lobby if the name field is not empty.
            bool nameFilled = nameBuf[0] != '\0'; //Cstr ends with \0! so if the first char is that there is no name!
            // Placing the widgets inside the empty box of the menuPic_AI_generated.png
            
            float panelW = win.x * 0.235f;
            float panelH = win.y * 0.40f;
            ImGui::SetCursorPos(ImVec2(win.x * 0.07f, win.y * 0.35f));

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.15f, 0.05f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.85f, 0.70f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.80f, 0.50f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.75f, 0.60f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.93f, 0.85f, 0.65f, 1.0f));
            // Left panel -- Hosting --
            ImGui::BeginChild("Host", ImVec2(panelW,panelH), true);
            ImGui::Text("HOST A PARTY");
            ImGui::TextWrapped("Create a room and share its code with your friends.");

            ImGui::BeginDisabled(!nameFilled);
            if(ImGui::Button("New Adventure", ImVec2(-1,50))){
                isHost = true;
                cuurentPlayer.name = nameBuf;
                screen = Screen::Lobby;
            }
            ImGui::EndDisabled();

            ImGui::EndChild();

            ImGui::SetCursorPos(ImVec2(win.x * 0.71f, win.y * 0.35f));

            // Right panel -- Joining --
            ImGui::BeginChild("Join", ImVec2(panelW,panelH), true);
            ImGui::Text("Join A PARTY");
            ImGui::TextWrapped("Enter the code you got from the host:");
            ImGui::InputTextWithHint("##code", "Room code", codeBuf, sizeof(codeBuf), ImGuiInputTextFlags_CharsUppercase);
            ImGui::BeginDisabled(!nameFilled);
            if(ImGui::Button("Join", ImVec2(-1, 50))){
                isHost = false;
                cuurentPlayer.name = nameBuf;
                screen = Screen::Lobby;
            }
            ImGui::EndDisabled();
            ImGui::EndChild();

            ImGui::PopStyleColor(5); //every color needs to be popped from the stack to set a new one.

            ImGui::End();
            
            // Help is TEMPORARY for now 
            if (showHelp)
            {
                ImGui::Begin("Help", &showHelp);
                ImGui::TextWrapped("Host a room to get a code and share it with your friends. "
                                "Or enter a friend's code on the right to join their room.");
                ImGui::End();
            }
        }
        // Lobby screen 
        else if (screen == Screen::Lobby){

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Lobby", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            if(ImGui::Button("Leave")){
                screen = Screen::Menu;
            }
            ImGui::Text("Lobby");
            ImGui::Text("Room code: %s", roomCode.c_str());
            ImGui::Text("Players:");
            // the name of the current player (you)
            ImGui::BulletText("%s - %s", cuurentPlayer.name.c_str(), roleName(cuurentPlayer.role));
            for (const auto& lb : lobbyPlayers)
                ImGui::BulletText("%s - %s", lb.name.c_str(), roleName(lb.role));

            

            bool dmTaken = false;
            for (LobbyPlayer p : lobbyPlayers )
            {
                if(p.role == LobbyRole::DM ) {dmTaken = true; }
            }
            // ROLE CARDS (color, dm lock, pics)
            LobbyRole roles[4] = {LobbyRole::Warrior, LobbyRole::Wizard, LobbyRole::Healer, LobbyRole::DM};
            for (LobbyRole role : roles)
            {
                bool locked = (role == LobbyRole::DM && dmTaken);
                bool chosen = (role == cuurentPlayer.role);
                GLuint tex = roleTexture(role);
                bool clicked = false;

                ImGui::BeginDisabled(locked);

                if (chosen)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 1.0f));
                }

                if (tex != 0)
                {
                    ImTextureID id = (ImTextureID)(intptr_t)tex;
                    clicked = ImGui::ImageButton(roleName(role), id, ImVec2(120, 120));
                }
                else
                {
                    clicked = ImGui::Button(roleName(role), ImVec2(120, 120));
                }

                if (chosen)
                {
                    ImGui::PopStyleColor();
                }

                ImGui::EndDisabled();

                if (clicked)
                {
                    cuurentPlayer.role = role;
                }

                ImGui::SameLine();
            }

            ImGui::NewLine();
            
            
            if (isHost){

                if (ImGui::Button("Start match", ImVec2(200, 40)))
                {
                    
                    try
                    {
                        // 1. Load Level 1 definition from JSON using GameData
                        LevelDefinition lvl1 = gameData->loadLevel(1);

                        // 2. Initialize the Match object using level max rounds
                        auto match = std::make_unique<Match>("match_1", lvl1.maxRounds);

                        // 3. Load the default Heroes for Level 1 (e.g., Warrior)
                        int heroCount = 0;
                        for (const auto& heroId : lvl1.defaultHeroes)
                        {
                            const auto& heroDef = gameData->getHero(heroId);
                            auto loadedHero = std::make_unique<Hero>(heroDef.id, heroDef.name, heroDef.toHeroType(), heroDef.maxMana);

                            // TEMPORARY until task 15 fills the deck from heroes.json
                            for (int i = 0; i < 10; ++i)
                                loadedHero->addToDeck(std::make_unique<HeroCard>("Strike", 1, HeroEffect::ATTACK, 5));

                            match->addPlayer(std::move(loadedHero));
                            ++heroCount;
                        }

                        // 3.5. Create and add the Dungeon Master (Required by Match rules!)
                        match->addPlayer(std::make_unique<DungeonMaster>("dm_1", "Dungeon Master", 10));

                        // 4. Load Level 1 Monsters from JSON: countPerHero x number of heroes (see data/README.md)
                        for (const auto& monsterEntry : lvl1.monsters)
                        {
                            const auto& monsterDef = gameData->getMonster(monsterEntry.monsterId);
                            for (int i = 0; i < monsterEntry.countPerHero * heroCount; ++i)
                                match->addMonster(std::make_unique<Monster>(monsterDef.name, monsterDef.maxHealth, monsterDef.boss));
                        }

                        // 5. Start the match and switch to the battle screen
                        match->start();
                        currentMatch = std::move(match);
                        screen = Screen::Battle;
                    }
                    catch (const std::exception& e)
                    {
                        errorMessage = std::string("Could not start the match:\n") + e.what();
                        std::cerr << errorMessage << "\n";
                    }
                }
            }
            else if(!isHost){
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Waiting for the host to start the match...");
            }



            ImGui::End();

        }
        // ****** Battle Screen ******
        else if(screen == Screen::Battle){
            // Full-screen battle layout
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Dungeon Party - Battle", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

            if (currentMatch)
            {
                // 1. TOP BAR: Status & Round Info
                ImGui::BeginChild("TopStatusBar", ImVec2(0, 40), true);
                if (auto* active = currentMatch->getActivePlayer()) {
                    if (ImGui::Button("End turn")) currentMatch->endTurn(*active);
                    ImGui::SameLine();
                }
                drawTurnIndicator(*currentMatch);


                ImGui::EndChild();

                // 2. MIDDLE BATTLEFIELD: Background & Combatants
                float middleHeight = ImGui::GetWindowHeight() - 220;
                ImGui::BeginChild("Battlefield", ImVec2(0, middleHeight), true);

                // Draw Level 1 Background PNG if loaded
                if (bgTexture != 0)
                {
                    // Draw background to fill the entire battlefield area
                    ImGui::SetCursorPos(ImVec2(0, 0));
                    ImGui::Image((ImTextureID)(intptr_t)bgTexture, ImVec2(ImGui::GetContentRegionAvail()));

                    // Reset cursor back to top-left so characters draw on top of the background
                    ImGui::SetCursorPos(ImVec2(10, 10));
                }

                Player* active = currentMatch->getActivePlayer();
                // Render all active heroes on the left side
                const auto& matchHeroes = currentMatch->getHeroes();
                for (size_t i = 0; i < matchHeroes.size(); ++i)
                {
                    if (i > 0) ImGui::SameLine();
                    Hero* h = matchHeroes[i];
                    if (h)
                    {
                        Character heroChar{
                            h->getName(),
                            h->getHealth(),
                            h->getType().maxHealth,
                            h->getMana(),
                            h->getMaxMana(),
                            textureOr0(heroTextures, h->getPlayerId())
                        };
                        drawCharacter(heroChar);
                    }
                }

                ImGui::SameLine(400); // Space between hero and monster

                // Render all monsters on the right side
                const auto& matchMonsters = currentMatch->getMonsters();
                int drawn = 0; // to count how many monsters are drawned
                for (size_t i = 0; i < matchMonsters.size(); ++i)
                {
                    const auto& m = matchMonsters[i];
                    if (!m || !m->isAlive()) continue; // to skip the dead monsters

                    if (drawn > 0) ImGui::SameLine();
                    drawn++;

                    Character monsterChar{
                        m->getName(),
                        m->getHealth(),
                        m->getMaxHealth(),
                        0, 0,
                        textureOr0(monsterTextures, m->getName())
                    };

                    if (drawCharacter(monsterChar) && selectedCard >= 0 && active)
                    {
                        try {
                            currentMatch->apply(active->getHandCard(selectedCard), *m);
                        }
                        catch (const std::exception& e) {
                            std::cerr << e.what() << "\n";
                        }
                        selectedCard = -1;
                    }
                }

                ImGui::EndChild();

                // Current mana of the active hero, used to decide which cards can be played
                int heroMana = 0;
                if (!matchHeroes.empty() && matchHeroes[0])
                    heroMana = matchHeroes[0]->getMana();

                // 3. BOTTOM TRAY: Card Hand & Deck Selection
                ImGui::BeginChild("CardTray", ImVec2(0, 0), true);

                float rowY = ImGui::GetCursorPosY() + 15.0f;

                if (active)
                {
                    for (std::size_t i = 0; i < active->getHandSize(); i++)
                    {
                        const Card& card = active->getHandCard(i);
                        const HeroCard* hc = dynamic_cast<const HeroCard*>(&card);

                        CardView view{
                            card.getName(),
                            card.getManaCost(),
                            hc ? hc->getEffect() : HeroEffect::ATTACK,
                            hc ? hc->getValue() : 0
                        };

                        if (i > 0) ImGui::SameLine();
                        // to distinguish the selected card
                        bool isSelected = ((int)i == selectedCard);
                        ImGui::SetCursorPosY(rowY - (isSelected ? 15.0f : 0.0f));

                        if (drawCard(view, heroMana >= view.manaCost, isSelected))
                            selectedCard = isSelected ? -1 : (int)i;
                    }
                }

                ImGui::EndChild();
            }
            else
            {
                ImGui::Text("No active match found. Please start from the menu.");
            }

            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}