#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include "Renderer.h"
#include "TurnIndicator.h"
#include "GameData.h"
#include <memory>

int main()
{
    // Minimal battle setup until the shared match setup flow is integrated.
    std::unique_ptr<Match> match;
    Hero* battleHero = nullptr;
    std::vector<Monster*> battleMonsters;
    try {
        GameData data(GameData::defaultDataDirectory());
        const auto level = data.loadLevel(1);
        match = std::make_unique<Match>("battle-1", level.maxRounds);
        auto hero = std::make_unique<Hero>("hero-1", "Hero", HeroType{HeroClass::WARRIOR, 30}, 10);
        battleHero = hero.get();
        match->addPlayer(std::move(hero));
        match->addPlayer(std::make_unique<DungeonMaster>("dm-1", "Dungeon Master", 3));
        for (int i = 1; i <= 3; ++i) {
            auto monster = std::make_unique<Monster>("Goblin " + std::to_string(i), 10);
            battleMonsters.push_back(monster.get());
            match->addMonster(std::move(monster));
        }
    } catch (const std::exception& e) {
        std::cerr << "Cannot set up match: " << e.what() << '\n';
        return -1;
    }

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

    Screen screen = Screen::Start;

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

            // ImGui::ShowDemoWindow();
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
                match->start();
                screen = Screen::Battle;
            }
            ImGui::End();
        }

        else {
            //test window
            ImGui::SetNextWindowSize(ImVec2(1100, 400));
            
            ImGui::Begin("Test");

            if (auto* active = match->getActivePlayer()) {
                if (ImGui::Button("End turn")) match->endTurn(*active);
            }
            drawTurnIndicator(*match);
            drawCharacter(Character{battleHero->getName(), battleHero->getHealth(),
                                    battleHero->getType().maxHealth, battleHero->getMana(), 10});
            ImGui::SameLine(300);
            for (size_t i = 0; i < battleMonsters.size(); i++) {
                if (i > 0) ImGui::SameLine();
                drawCharacter(Character{battleMonsters[i]->getName(), battleMonsters[i]->getHealth(), 10, 0, 0});
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