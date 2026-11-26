# Match result database

A match result has one row in `match_results` and one row in `match_players` per participant. The foreign key `match_players.match_id` links the tables. For logged-in players, `player_id` is the permanent account ID (for example `P-K7RM2Q9X`), shared across their matches. Room seat IDs such as `p1` are only used when login is disabled for tests. Older saved results keep their original room IDs; these cannot be linked to accounts reliably.

Store the completion time in UTC as ISO 8601 text, for example `2026-11-16T13:00:00Z`. Store the room member's display name and selected role as they were in that match. The match ID is a persistent identifier for one match.

## Applying migrations

Read `PRAGMA user_version` before running migrations. Apply only numbered scripts newer than the database version, in ascending order. Enable `PRAGMA foreign_keys = ON` for every connection before starting a transaction.

To initialize a new database from the repository root with the SQLite CLI:

```sh
sqlite3 -bail -cmd "PRAGMA foreign_keys = ON;" match-results.sqlite ".read database/migrations/001_match_results.sql"
```

Migration 001 expects version 0 and commits the schema and version 1 together.

The dedicated server initializes the database at startup. It requires SQLite development files at build time. The fourth argument is the accounts file, as in the login server. Pass an optional fifth argument to set the results database path: `dungeon_server [port] [dataFolder] [turnSeconds] [accountsFile] [databasePath]`. The default is match-results.sqlite in the server working directory.


## Hosting from the game

Hosting on this computer saves completed matches too. The embedded server uses
the same `makeMatchResultSink` as the dedicated server, with `match-results.sqlite`
in the project directory, next to `accounts.json`. The callback owns the database
connection until the server is destroyed. Both hosting modes use permanent
account IDs and the same versioned schema. Runtime results files are ignored by Git.

The network test finishes authenticated matches on all three levels through DM
attacks, then destroys the server and reopens the SQLite file to verify the saved
results remain available.
