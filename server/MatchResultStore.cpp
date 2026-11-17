#include "MatchResultStore.h"
#include "MatchResultsMigration.h"

#include <sqlite3.h>

#include <memory>
#include <stdexcept>

namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

void require(sqlite3* db, int actual, int expected = SQLITE_OK) {
    if (actual != expected) throw std::runtime_error(sqlite3_errmsg(db));
}

void execute(sqlite3* db, const char* sql) {
    require(db, sqlite3_exec(db, sql, nullptr, nullptr, nullptr));
}

Statement prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    require(db, sqlite3_prepare_v2(db, sql, -1, &statement, nullptr));
    return Statement(statement, sqlite3_finalize);
}

void bindText(sqlite3* db, const Statement& statement, int index, const std::string& value) {
    require(db, sqlite3_bind_text(statement.get(), index, value.data(),
                                  static_cast<int>(value.size()), SQLITE_TRANSIENT));
}

std::string readText(sqlite3_stmt* statement, int column) {
    const auto* text = sqlite3_column_text(statement, column);
    return {reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(sqlite3_column_bytes(statement, column))};
}

} // namespace

MatchResultStore::MatchResultStore(const std::string& path) {
    sqlite3* connection = nullptr;
    const int opened = sqlite3_open(path.c_str(), &connection);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> guard(connection, sqlite3_close);
    if (!connection) throw std::runtime_error("Could not allocate SQLite connection");
    require(connection, opened);
    require(connection, sqlite3_busy_timeout(connection, 1000));
    execute(connection, "PRAGMA foreign_keys = ON");
    int version;
    {
        auto query = prepare(connection, "PRAGMA user_version");
        require(connection, sqlite3_step(query.get()), SQLITE_ROW);
        version = sqlite3_column_int(query.get(), 0);
    }
    if (version == 0) execute(connection, matchResultsMigration);
    else if (version != 1) throw std::runtime_error("Unsupported match result database version");
    db = guard.release();
}

MatchResultStore::~MatchResultStore() {
    sqlite3_close(db);
}

void MatchResultStore::save(const std::string& matchId, Side winner,
                            const std::vector<RoomServer::ResultPlayer>& players) {
    execute(db, "BEGIN IMMEDIATE");
    try {
        auto result = prepare(db, "INSERT INTO match_results (match_id, winner, finished_at_utc) "
                                  "VALUES (?, ?, strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))");
        bindText(db, result, 1, matchId);
        bindText(db, result, 2, winner == Side::HEROES ? "HEROES" : "DUNGEON_MASTER");
        require(db, sqlite3_step(result.get()), SQLITE_DONE);

        auto player = prepare(db, "INSERT INTO match_players (match_id, player_id, display_name, role) "
                                  "VALUES (?, ?, ?, ?)");
        for (const auto& entry : players) {
            bindText(db, player, 1, matchId);
            bindText(db, player, 2, entry.id);
            bindText(db, player, 3, entry.name);
            bindText(db, player, 4, entry.role);
            require(db, sqlite3_step(player.get()), SQLITE_DONE);
            require(db, sqlite3_reset(player.get()));
        }
        execute(db, "COMMIT");
    } catch (...) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

std::optional<MatchResultStore::Result> MatchResultStore::load(const std::string& matchId) const {
    auto query = prepare(db, "SELECT r.winner, r.finished_at_utc, p.player_id, p.display_name, p.role "
                             "FROM match_results AS r LEFT JOIN match_players AS p USING (match_id) "
                             "WHERE r.match_id = ? ORDER BY p.player_id");
    bindText(db, query, 1, matchId);
    int status = sqlite3_step(query.get());
    if (status == SQLITE_DONE) return std::nullopt;
    require(db, status, SQLITE_ROW);

    Result result{matchId, readText(query.get(), 0) == "HEROES" ? Side::HEROES : Side::DUNGEON_MASTER,
                  readText(query.get(), 1), {}};
    do {
        if (sqlite3_column_type(query.get(), 2) != SQLITE_NULL)
            result.players.push_back({readText(query.get(), 2), readText(query.get(), 3), readText(query.get(), 4)});
        status = sqlite3_step(query.get());
    } while (status == SQLITE_ROW);
    require(db, status, SQLITE_DONE);
    return result;
}
