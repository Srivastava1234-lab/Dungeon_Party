// dungeon_server - headless room server (no window, no OpenGL).
//
// Usage:
//   dungeon_server                     port 7777, data folder baked in by CMake
//   dungeon_server <port>
//   dungeon_server <port> <dataFolder>  use this on a cloud VM where the build path doesn't exist
//   dungeon_server <port> <dataFolder> <seconds>  every turn lasts <seconds> (0 = rules.json)
//   dungeon_server <port> <dataFolder> <seconds> <accountsFile>  where player accounts are saved
//                                                                 (default: accounts.json in the current folder)
//   dungeon_server <port> <dataFolder> <seconds> <accountsFile> <databasePath>  match results SQLite file
//                                                                            (default: match-results.sqlite)
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
#include "MatchResultStore.h"

namespace {
std::atomic<bool> running{true};
void stop(int) { running = false; }
}

int main(int argc, char** argv) {
    std::cout << std::unitbuf; // flush every line, so logs show up immediately (systemd, docker, files)
    std::uint16_t port = net::DEFAULT_PORT;
    std::string dataDir = GameData::defaultDataDirectory();
    int turnSeconds = 0; // 0 = use the time limits from rules.json
    std::string accountsFile = "accounts.json";
    std::string databasePath = "match-results.sqlite";
    try {
        if (argc > 1) port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        if (argc > 2) dataDir = argv[2];
        if (argc > 3) turnSeconds = std::stoi(argv[3]);
        if (argc > 4) accountsFile = argv[4];
        if (argc > 5) databasePath = argv[5];
    } catch (const std::exception&) {
        std::cerr << "Usage: dungeon_server [port] [dataFolder] [turnSeconds] [accountsFile] [databasePath]\n";
        return 2;
    }

    std::signal(SIGINT, stop);  // Ctrl+C
    std::signal(SIGTERM, stop); // systemd / docker stop

    try {
        GameData gameData(dataDir);
        MatchResultStore results(databasePath);
        RoomServer server(gameData, port, turnSeconds,
                          std::chrono::seconds(net::EMPTY_ROOM_GRACE_SECONDS),
                          [&results](const auto& matchId, Side winner, const auto& players) {
                              results.save(matchId, winner, players);
                          });
        server.setAccountsFile(accountsFile);
        std::cout << "Player accounts: " << accountsFile << "\n";
        std::cout << "Match results: " << databasePath << "\n";
        std::cout << "Dungeon Party server listening on UDP port " << port
                  << " (protocol v" << net::PROTOCOL_VERSION
                  << ", reconnect grace " << net::EMPTY_ROOM_GRACE_SECONDS << "s, Ctrl+C to stop)\n";
        if (turnSeconds > 0) std::cout << "Every turn lasts " << turnSeconds << " seconds\n";
        while (running) server.poll(100);
        std::cout << "Shutting down\n";
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
