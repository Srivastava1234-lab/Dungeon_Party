#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include "Renderer.h"

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

    // test:
    Screen screen = Screen::Start;
    Character hero{"Hero", 20, 30, 4, 10};
    std::vector<Character> monsters = {
    {"Goblin 1", 10, 10, 5, 5},
    {"Goblin 2", 7, 10, 3, 5},
    {"Goblin 3", 10, 10, 5, 5}
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
                screen = Screen::Battle;
            }
            ImGui::End();
        }

        else {
            //test window
            ImGui::SetNextWindowSize(ImVec2(1100, 400));
            
            ImGui::Begin("Test");

            drawCharacter(hero);
            ImGui::SameLine(300);
            for (size_t i = 0; i < monsters.size(); i++) {
                if (i > 0) ImGui::SameLine();
                drawCharacter(monsters[i]);
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