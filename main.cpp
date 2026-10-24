#include <glad/glad.h>
#include <GLFW/glfw3.h>
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

    // Create and connect ImGui to window
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize GLAD (load OpenGL function pointers)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    std::cout << "OpenGL loaded: " << glGetString(GL_VERSION) << "\n";

    // Test state and match setup
    Screen screen = Screen::Start;
    std::unique_ptr<Match> currentMatch = nullptr;

    Character hero{"Hero", 20, 30, 4, 10};
    std::vector<Character> monsters = {
        {"Goblin 1", 10, 10, 5, 5},
        {"Goblin 2", 7, 10, 3, 5},
        {"Goblin 3", 10, 10, 5, 5}
    };
    std::vector<CardView> hand = {
    {"Strike",     1, HeroEffect::ATTACK, 5},
    {"Heavy blow", 5, HeroEffect::ATTACK, 14},
    {"Heal",  2, HeroEffect::HEAL, 6}
};

    // Initialize GameData loading from the data/ folder
    GameData gameData(GameData::defaultDataDirectory());

    // Load PNG Textures at startup
    GLuint warriorTexture = 0;
    GLuint monsterTexture = 0;
    GLuint bgTexture = 0;
    int width, height;

    LoadTextureFromFile("assets/Warrior_Artwork.png", &warriorTexture, &width, &height);
    LoadTextureFromFile("assets/Monster_Artwork.png", &monsterTexture, &width, &height);
    LoadTextureFromFile("assets/Level_1_Artwork.png", &bgTexture, &width, &height);

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
            GLuint warriorTexture = 0;
            int wWidth, wHeight;
            LoadTextureFromFile("assets/warrior.png", &warriorTexture, &wWidth, &wHeight);
            ImGuiIO &io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 220));

            ImGui::Begin("Start", nullptr,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowFontScale(2.0f);
            ImGui::Text("DUNGEON PARTY");
            if (ImGui::Button("Start match", ImVec2(-1, 70)))
            {
                // 1. Load Level 1 definition from JSON using GameData
                LevelDefinition lvl1 = gameData.loadLevel(1);

                // 2. Initialize the Match object using level max rounds
                currentMatch = std::make_unique<Match>("match_1", lvl1.maxRounds);

                // 3. Load the default Hero for Level 1 (e.g., Warrior)
                if (!lvl1.defaultHeroes.empty())
                {
                    const auto& heroDef = gameData.getHero(lvl1.defaultHeroes[0]);
                    auto loadedHero = std::make_unique<Hero>(heroDef.id, heroDef.name, heroDef.toHeroType(), heroDef.maxMana);

                    currentMatch->addPlayer(std::move(loadedHero));
                }

                // 3.5. Create and add the Dungeon Master (Required by Match rules!)
                auto dungeonMaster = std::make_unique<DungeonMaster>("dm_1", "Dungeon Master", 10);
                currentMatch->addPlayer(std::move(dungeonMaster));

                // 4. Load Level 1 Monsters dynamically from JSON
                for (const auto& monsterEntry : lvl1.monsters)
                {
                    const auto& monsterDef = gameData.getMonster(monsterEntry.monsterId);
                    for (int i = 0; i < monsterEntry.count; ++i)
                    {
                        auto monster = std::make_unique<Monster>(monsterDef.name, monsterDef.maxHealth, monsterDef.boss);
                        currentMatch->addMonster(std::move(monster));
                    }
                }

                // 5. Start the match and switch to the battle screen
                currentMatch->start();
                screen = Screen::Battle;
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
                ImGui::Text("Round: %d / %d", currentMatch->getRound(), 20);
                ImGui::SameLine(200);
                ImGui::Text("Phase: Active Turn");
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
                            warriorTexture // Pass the loaded Warrior texture!
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
                            m->getHealth(),
                            0, 0,
                            monsterTexture // Pass the loaded Goblin texture!
                        };
                        drawCharacter(monsterChar);
                    }
                }

                ImGui::EndChild();

                // Current mana of the active hero, used to decide which cards can be played
                int heroMana = 0;
                if (!matchHeroes.empty() && matchHeroes[0])
                    heroMana = matchHeroes[0]->getMana();

                // 3. BOTTOM TRAY: Card Hand & Deck Selection
                ImGui::BeginChild("CardTray", ImVec2(0, 0), true);

                for (size_t i = 0; i < hand.size();i++){
                    if (i > 0) ImGui::SameLine();
                    drawCard(hand[i], heroMana >= hand[i].manaCost);
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