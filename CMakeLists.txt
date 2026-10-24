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

# Only build the GLFW library itself (no docs/tests/examples), and use X11 on Linux
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

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

#IMGUI //ss
FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.tar.gz
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

add_library(glad_lib STATIC external/glad/src/glad.c)
target_include_directories(glad_lib PUBLIC external/glad/include)

add_library(game_data STATIC
        src/GameData.cpp
)
target_include_directories(game_data PUBLIC include)
target_link_libraries(game_data PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(game_data PRIVATE
        DUNGEON_PARTY_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

#Add all source files
add_executable(${PROJECT_NAME}
        main.cpp
        src/Card.cpp
        src/DungeonMaster.cpp
        src/Hero.cpp
        src/Match.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Renderer.cpp
        src/TurnIndicator.cpp
        src/Trap.cpp
)

# Lets the game find assets/ no matter which folder it is started from
target_compile_definitions(${PROJECT_NAME} PRIVATE
        DUNGEON_PARTY_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        glm::glm
        glad_lib
        game_data
        imgui
)

# validate_data Tool & Tests (SCRUM-5)

add_executable(console_game
        console_main.cpp
        src/Card.cpp
        src/DungeonMaster.cpp
        src/Hero.cpp
        src/Match.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Trap.cpp
)
target_link_libraries(console_game PRIVATE game_data)

add_executable(validate_data
        tools/validate_data.cpp
)
target_link_libraries(validate_data PRIVATE game_data)

enable_testing()
add_test(NAME data_validation COMMAND validate_data)
add_executable(turn_indicator_tests
        tests/turn_indicator_tests.cpp
        src/Card.cpp
        src/DungeonMaster.cpp
        src/Hero.cpp
        src/Match.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Trap.cpp
)
target_include_directories(turn_indicator_tests PRIVATE include)
add_test(NAME turn_indicator COMMAND turn_indicator_tests)

# Kagan's model tests (were in tests/ but never built)
add_executable(model_tests
        tests/model_tests.cpp
        src/Card.cpp
        src/DungeonMaster.cpp
        src/Hero.cpp
        src/Match.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Trap.cpp
)
target_include_directories(model_tests PRIVATE include)
add_test(NAME model COMMAND model_tests)
# model_tests uses assert(), which Release builds switch off - keep it on for this target
target_compile_options(model_tests PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
