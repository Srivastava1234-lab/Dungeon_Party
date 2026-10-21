# =============================================================================
# Dungeon Party - CMakeLists.txt for branch hulya/SCRUM-5-data-format
# -----------------------------------------------------------------------------
# This branch is based on kagan/game-logic, which has no CMakeLists.txt.
# The CMakeLists.txt on main still lists the old skeleton files (Game.cpp,
# Deck.cpp, Boss.cpp, ...) that do not exist on this branch, so it cannot be
# used here unchanged.
#
# This file therefore only builds what SCRUM-5 adds:
#   - game_data     : small library with the JSON data loader (src/GameData.cpp)
#   - validate_data : command-line tool that checks the files in data/
#
# It does NOT build the game window (main.cpp) or Kagan's game logic.
# Joining everything into one build belongs to the integration task
# (Sprint 1, task 7) - see the notes at the end of this file.
# =============================================================================

cmake_minimum_required(VERSION 3.20)    # same minimum version as main's CMakeLists.txt
project(Dungeon_Party LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)              # same C++ standard as main
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Newer CMake versions (3.24+) print a warning about the time stamps of
# downloaded archives unless this policy is set. Older versions skip this.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# -----------------------------------------------------------------------------
# nlohmann/json 3.11.3 - the JSON library used to read the files in data/
# -----------------------------------------------------------------------------
# - Header-only C++ library with an MIT licence (open source, as required by
#   the Vision document).
# - The version is fixed to 3.11.3, so every team member builds with exactly the
#   same library (risk 10 in the risk management plan).
# - We download the small release archive (about 110 KB) instead of cloning the
#   whole Git repository, which is much larger.
# - URL_HASH lets CMake check that the downloaded file is complete and unchanged.
# - Only src/GameData.cpp uses this library. GameData.h does not include it.
FetchContent_Declare(
        json
        URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(json)        # creates the target nlohmann_json::nlohmann_json

# -----------------------------------------------------------------------------
# game_data - the data loader (SCRUM-5)
# -----------------------------------------------------------------------------
# A small library, so every program that needs the game data can simply link
# to it: validate_data now, the game executable later (one line, see the end).
add_library(game_data STATIC
        src/GameData.cpp
)

# include/ contains GameData.h and Kagan's Types.h (GameData.h needs Types.h).
# PUBLIC: every program that links game_data can also #include "GameData.h".
target_include_directories(game_data PUBLIC include)

# PRIVATE: only GameData.cpp uses the JSON library; programs that link
# game_data do not need it.
target_link_libraries(game_data PRIVATE nlohmann_json::nlohmann_json)

# Tells GameData::defaultDataDirectory() where the data/ folder is.
# It is the full path of <project folder>/data, so the data is found no matter
# which folder the program is started from (CLion, VS Code, terminal), and
# JSON files can be changed without rebuilding.
# NOTE: a release build (Sprint 3) will have to ship the data/ folder next to
# the program instead. This will be decided in the release task.
target_compile_definitions(game_data PRIVATE
        DUNGEON_PARTY_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

# -----------------------------------------------------------------------------
# validate_data - checks the files in data/ (SCRUM-5)
# -----------------------------------------------------------------------------
# Only needs game_data: no GLFW, GLAD, GLM or ImGui, so it builds in seconds.
add_executable(validate_data
        tools/validate_data.cpp
)
target_link_libraries(validate_data PRIVATE game_data)

# Registers the data check as a test, so running "ctest" also checks the data.
# Exit code 0 = test passed, exit code 1 = test failed.
enable_testing()
add_test(NAME data_validation COMMAND validate_data)

# -----------------------------------------------------------------------------
# Notes for integration into the main build (Sprint 1, task 7)
# -----------------------------------------------------------------------------
# When this branch is combined with main / Soroush's GUI, keep everything above
# (nlohmann/json, game_data, validate_data, the test) and add main's GLFW, GLAD,
# GLM and ImGui blocks and the game executable. The game then only needs:
#
#     target_link_libraries(Dungeon_Party PRIVATE game_data)
#
# to use GameData.h, the loader and the data folder path.
# =============================================================================
