cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(Dungeon_Party)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

#Include headers folder
include_directories(include)
include_directories(external/glad/include)

#GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.9
)
FetchContent_MakeAvailable(glfw)

#Compile GLAD locally to avoid python issue on fetch and 17 headaches
add_library(glad_lib STATIC external/glad/src/glad.c)
target_include_directories(glad_lib PUBLIC external/glad/include)

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
        src/Card.cpp
        src/Renderer.cpp
        src/Hero.cpp
        src/Match.cpp
        src/DungeonMaster.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Trap.cpp
)

#Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad_lib glm)

#Link OpenGL for Windows
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()