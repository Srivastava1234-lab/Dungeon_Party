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

    auto textureOr0 = [](const std::map<std::string, GLuint>& textures, const std::string& key) -> GLuint {
        auto it = textures.find(key);
        return it == textures.end() ? 0 : it->second;
    };

    // Main loop clear screen to a color until window closed
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.12f, 0.34f, 0.56f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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
            else if (ImGui::Button("Start match", ImVec2(-1, 70)))
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
                        match->addPlayer(std::make_unique<Hero>(heroDef.id, heroDef.name, heroDef.toHeroType(), heroDef.maxMana));
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
            ImGui::End();
        }
        else {
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
                for (size_t i = 0; i < matchMonsters.size(); ++i)
                {
                    if (i > 0) ImGui::SameLine();
                    const auto& m = matchMonsters[i];
                    if (m)
                    {
                        Character monsterChar{
                            m->getName(),
                            m->getHealth(),
                            m->getMaxHealth(),
                            0, 0,
                            textureOr0(monsterTextures, m->getName())
                        };
                        drawCharacter(monsterChar);
                    }
                }

                ImGui::EndChild();

                // 3. BOTTOM TRAY: Card Hand & Deck Selection
                ImGui::BeginChild("CardTray", ImVec2(0, 0), true);
                ImGui::Text("Card Hand & Deck Tray (Coming soon in Card/Deck model task)");
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