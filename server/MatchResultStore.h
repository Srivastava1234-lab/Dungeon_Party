#pragma once

#include "RoomServer.h"

#include <optional>
struct sqlite3;

class MatchResultStore {
public:
    struct Result {
        std::string matchId;
        Side winner;
        std::string finishedAtUtc;
        std::vector<RoomServer::ResultPlayer> players;
    };

    std::optional<Result> load(const std::string& matchId) const;

    explicit MatchResultStore(const std::string& path);
    ~MatchResultStore();
    MatchResultStore(const MatchResultStore&) = delete;
    MatchResultStore& operator=(const MatchResultStore&) = delete;

    void save(const std::string& matchId, Side winner,
              const std::vector<RoomServer::ResultPlayer>& players);

private:
    sqlite3* db = nullptr;
};

// The callback owns the database connection for as long as the server uses it.
RoomServer::ResultSink makeMatchResultSink(const std::string& path);
