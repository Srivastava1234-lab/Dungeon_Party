# Dungeon_Party
A card based dungeon game

## Building and running

This is a CMake project. Don't use VS Code's "Run C/C++ File" / Code Runner play button:
it compiles only the open file and fails with "cannot open source file glad/glad.h" or "imgui.h".

### VS Code
1. Install the **C/C++** and **CMake Tools** extensions, plus CMake and a compiler
   (Windows: Visual Studio Build Tools "Desktop development with C++", or MSYS2 MinGW).
2. Open the repository folder (the one containing `CMakeLists.txt`), not a subfolder.
3. When asked, pick a kit (compiler). The first configure downloads GLFW, GLM, ImGui
   and nlohmann/json, so it needs internet and takes a minute.
4. In the status bar, set the launch target to `Dungeon_Party` and press the play/debug button.

### Command line
```
cmake -S . -B build
cmake --build build
./build/Dungeon_Party          (Windows: build\Debug\Dungeon_Party.exe with Visual Studio)
```

### Other targets
- `console_game`: text version of the match logic
- `validate_data`: checks the JSON files in `data/` (run after editing them)
- `model_tests`, `turn_indicator_tests`: run all tests with `ctest --test-dir build`

The game finds `data/` and `assets/` through paths CMake bakes in, so it works whichever
folder it is started from. If you move the project folder, re-run CMake configure.
