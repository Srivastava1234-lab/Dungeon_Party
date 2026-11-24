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
        src/Warrior.cpp
        src/Healer.cpp
        src/Wizard.cpp
        src/TimedEffects.cpp
)
target_include_directories(game_logic PUBLIC include)
target_link_libraries(game_logic PUBLIC nlohmann_json::nlohmann_json)

# Client-side networking
add_library(net_client STATIC net/NetClient.cpp)
target_include_directories(net_client PUBLIC net)
target_link_libraries(net_client PUBLIC enet nlohmann_json::nlohmann_json)

# Room logic as a library (embedded directly into the game for Listen Server hosting!)
add_library(room_server STATIC server/RoomServer.cpp)
target_include_directories(room_server PUBLIC server)
target_link_libraries(room_server PUBLIC enet nlohmann_json::nlohmann_json game_logic game_data)

set(DUNGEON_PARTY_DEFAULT_SERVER "" CACHE STRING "Optional default server host[:port] pre-filled in the client")

# Main Unified Game Executable (Local + Listen Server Host + Online Join)
add_executable(${PROJECT_NAME}
        main.cpp
        src/Renderer.cpp
        src/Audio.cpp
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
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread m) # miniaudio on Linux
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE
        DUNGEON_PARTY_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        DUNGEON_PARTY_DEFAULT_SERVER="${DUNGEON_PARTY_DEFAULT_SERVER}"
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
find_package(SQLite3 REQUIRED)
set(match_results_migration "${CMAKE_CURRENT_SOURCE_DIR}/database/migrations/001_match_results.sql")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${match_results_migration}")
file(READ "${match_results_migration}" match_results_sql)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/MatchResultsMigration.h"
        "inline constexpr char matchResultsMigration[] = R\"SQL(${match_results_sql})SQL\";\n")
target_sources(dungeon_server PRIVATE server/MatchResultStore.cpp)
target_include_directories(dungeon_server PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(dungeon_server PRIVATE room_server SQLite::SQLite3)

add_executable(match_result_tests tests/match_result_tests.cpp server/MatchResultStore.cpp)
target_include_directories(match_result_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(match_result_tests PRIVATE room_server SQLite::SQLite3)
add_test(NAME match_results_by_id COMMAND match_result_tests)

# SCRUM-46 (partial): current DM attack cards and queued monster actions.
# Links game_logic instead of listing the source files, so new game files (like the hero
# subclasses) are picked up automatically.
add_executable(monster_card_tests tests/monster_card_tests.cpp)
target_link_libraries(monster_card_tests PRIVATE game_logic)
foreach(case_name deferred_damage order mana turn_authority invalid_targets invalid_cards turn_limit queue_cleared defeated_participants match_end spell_targets frost_bolt scheduled_boss)
    add_test(NAME monster_card_${case_name} COMMAND monster_card_tests ${case_name})
endforeach()

add_executable(net_tests tests/net_tests.cpp)
target_sources(net_tests PRIVATE server/MatchResultStore.cpp)
target_include_directories(net_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(net_tests PRIVATE room_server net_client SQLite::SQLite3)
add_test(NAME network COMMAND net_tests)

# SCRUM-59: existing card interaction checks, including Release builds.
foreach(test_name shield_test weakness_test support_card_test burn_test model_tests)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE game_logic)
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
add_executable(buff_tests tests/buff_tests.cpp)
target_link_libraries(buff_tests PRIVATE game_logic game_data)
add_test(NAME battle_cry COMMAND buff_tests)

add_executable(card_data_tests tests/card_data_tests.cpp)
target_link_libraries(card_data_tests PRIVATE game_data nlohmann_json::nlohmann_json)
foreach(case_name valid_effects missing_file malformed_json structure required_fields field_types numbers ids enums deck_references)
    add_test(NAME card_data_${case_name} COMMAND card_data_tests ${case_name} "${CMAKE_CURRENT_SOURCE_DIR}/data")
endforeach()

add_executable(healing_tests tests/healing_tests.cpp)
target_link_libraries(healing_tests PRIVATE game_logic)
foreach(case_name amount max_cap full_health nonpositive large_amount card_effect role_and_target)
    add_test(NAME healing_${case_name} COMMAND healing_tests ${case_name})
endforeach()

# Sprint 3 task 38: register the remaining existing test programs.
add_executable(drawing_mana_tests tests/drawing_mana_tests.cpp)
target_link_libraries(drawing_mana_tests PRIVATE game_logic)
foreach(case_name drawing mana_boundaries spending_turns rejected_play)
    add_test(NAME drawing_mana_${case_name} COMMAND drawing_mana_tests ${case_name})
endforeach()

add_executable(turn_indicator_tests tests/turn_indicator_tests.cpp)
target_link_libraries(turn_indicator_tests PRIVATE game_logic)
add_test(NAME turn_indicator COMMAND turn_indicator_tests)
