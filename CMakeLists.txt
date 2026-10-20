cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(Dungeon_Party)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

#Include headers folder
include_directories(include)

#GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.9
)
FetchContent_MakeAvailable(glfw)

#GLAD via FetchContent
FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG        v0.1.36
)
FetchContent_MakeAvailable(glad)

#GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)

#Add all source files
add_executable(${PROJECT_NAME}
        main.cpp
        src/Game.cpp
        src/Hero.cpp
        src/Card.cpp
        src/Deck.cpp
        src/Renderer.cpp
        src/Boss.cpp
)

#Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad glm)

#Link OpenGL for Windows
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()