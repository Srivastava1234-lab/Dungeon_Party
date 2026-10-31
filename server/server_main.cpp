// dungeon_server - headless room server (no window, no OpenGL).
//
// Usage:
//   dungeon_server                     port 7777, data folder baked in by CMake
//   dungeon_server <port>
//   dungeon_server <port> <dataFolder>  use this on a cloud VM where the build path doesn't exist
//
// Players on other networks connect to this machine's public IP (or domain) on UDP <port>.

#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string>

#include "../net/Protocol.h"
#include "GameData.h"
#include "RoomServer.h"

namespace {
std::atomic<bool> running{true};
void stop(int) { running = false; }
}

int main(int argc, char** argv) {
    std::cout << std::unitbuf; // flush every line, so logs show up immediately (systemd, docker, files)
    std::uint16_t port = net::DEFAULT_PORT;
    std::string dataDir = GameData::defaultDataDirectory();
    try {
        if (argc > 1) port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        if (argc > 2) dataDir = argv[2];
    } catch (const std::exception&) {
        std::cerr << "Usage: dungeon_server [port] [dataFolder]\n";
        return 2;
    }

    std::signal(SIGINT, stop);  // Ctrl+C
    std::signal(SIGTERM, stop); // systemd / docker stop

    try {
        GameData gameData(dataDir);
        RoomServer server(gameData, port);
        std::cout << "Dungeon Party server listening on UDP port " << port << " (Ctrl+C to stop)\n";
        while (running) server.poll(100);
        std::cout << "Shutting down\n";
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
