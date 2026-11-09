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
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.5
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

# ===================== Online play & Game Logic Libraries =====================

# ENet: small reliable-UDP networking library
FetchContent_Declare(
        enet
        URL      https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz
        URL_HASH SHA256=28603c895f9ed24a846478180ee72c7376b39b4bb1287b73877e5eae7d96b0dd
)
FetchContent_MakeAvailable(enet)
target_include_directories(enet PUBLIC ${enet_SOURCE_DIR}/include)
if(WIN32)
    target_link_libraries(enet ws2_32 winmm)
endif()

# The game rules as one library, shared by the game, server, and tests
add_library(game_logic STATIC
        src/Card.cpp
        src/DungeonMaster.cpp
        src/GameState.cpp
        src/Hero.cpp
        src/Match.cpp
        src/Monster.cpp
        src/Player.cpp
        src/Trap.cpp
)
target_include_directories(game_logic PUBLIC include)

# Client-side networking
add_library(net_client STATIC net/NetClient.cpp)
target_include_directories(net_client PUBLIC net)
target_link_libraries(net_client PUBLIC enet nlohmann_json::nlohmann_json)

# Room logic as a library (embedded directly into the game for Listen Server hosting!)
add_library(room_server STATIC server/RoomServer.cpp)
target_include_directories(room_server PUBLIC server)
target_link_libraries(room_server PUBLIC enet nlohmann_json::nlohmann_json game_logic game_data)

# Main Unified Game Executable (Local + Listen Server Host + Online Join)
add_executable(${PROJECT_NAME}
        main.cpp
        src/Renderer.cpp
        include/stb_image.h
)
target_link_libraries(${PROJECT_NAME} PRIVATE
        net_client
        room_server
        game_data
        game_logic
        glfw
        glm::glm
        glad_lib
        imgui
        nlohmann_json::nlohmann_json
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
        DUNGEON_PARTY_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Optional standalone executables & tests
add_executable(console_game console_main.cpp)
target_link_libraries(console_game PRIVATE game_logic game_data)

add_executable(validate_data tools/validate_data.cpp)
target_link_libraries(validate_data PRIVATE game_data)

enable_testing()
add_test(NAME data_validation COMMAND validate_data)

add_executable(combat_tests tests/combat_tests.cpp)
target_include_directories(combat_tests PRIVATE include)
target_link_libraries(combat_tests PRIVATE game_logic)
foreach(case_name attack_damage invalid_targets monster_damage victory round_limit)
    add_test(NAME combat_${case_name} COMMAND combat_tests ${case_name})
endforeach()

add_executable(dungeon_server server/server_main.cpp)
target_link_libraries(dungeon_server PRIVATE room_server)

add_executable(dungeon_online online_main.cpp src/Renderer.cpp)
target_link_libraries(dungeon_online PRIVATE net_client room_server game_data game_logic glfw glad_lib imgui)
target_compile_definitions(dungeon_online PRIVATE DUNGEON_PARTY_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(net_tests tests/net_tests.cpp)
target_link_libraries(net_tests PRIVATE room_server net_client)
add_test(NAME network COMMAND net_tests)
