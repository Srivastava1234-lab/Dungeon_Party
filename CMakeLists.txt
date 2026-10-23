cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(Dungeon_Party LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# External Libraries: GLFW, GLM, nlohmann/json

FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        glm
        URL https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.tar.gz
)
FetchContent_MakeAvailable(glm)

# nlohmann/json for SCRUM-5 data loading
FetchContent_Declare(
        json
        URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(json)

# Local GLAD Library Setup

add_library(glad_lib STATIC external/glad/src/glad.c)
target_include_directories(glad_lib PUBLIC external/glad/include)

# game_data Library (SCRUM-5 JSON Loader)

add_library(game_data STATIC
        src/GameData.cpp
)
target_include_directories(game_data PUBLIC include)
target_link_libraries(game_data PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(game_data PRIVATE
        DUNGEON_PARTY_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

# Main Game Executable

add_executable(${PROJECT_NAME}
        main.cpp
        src/Card.cpp
        src/DungeonMaster.cpp
        src/Hero.cpp
        src/Match.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Renderer.cpp
        src/Trap.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        glm::glm
        glad_lib
        game_data
)

# validate_data Tool & Tests (SCRUM-5)

add_executable(validate_data
        tools/validate_data.cpp
)
target_link_libraries(validate_data PRIVATE game_data)

enable_testing()
add_test(NAME data_validation COMMAND validate_data)