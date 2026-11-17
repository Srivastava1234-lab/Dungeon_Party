# Match result database

A match result has one row in `match_results` and one row in `match_players` per participant. The foreign key `match_players.match_id` links the tables. A player ID is scoped to a match, so the server can reuse IDs such as `p1` in other matches.

Store the completion time in UTC as ISO 8601 text, for example `2026-11-16T13:00:00Z`. Store the room member's display name and selected role as they were in that match. The match ID is a persistent identifier for one match.

## Applying migrations

Read `PRAGMA user_version` before running migrations. Apply only numbered scripts newer than the database version, in ascending order. Enable `PRAGMA foreign_keys = ON` for every connection before starting a transaction.

To initialize a new database from the repository root with the SQLite CLI:

```sh
sqlite3 -bail -cmd "PRAGMA foreign_keys = ON;" match-results.sqlite ".read database/migrations/001_match_results.sql"
```

Migration 001 expects version 0 and commits the schema and version 1 together.

The dedicated server initializes the database at startup. It requires SQLite development files at build time. Pass an optional third argument to set the database path. The default is match-results.sqlite in the server working directory.
