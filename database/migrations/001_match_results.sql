BEGIN IMMEDIATE;

CREATE TABLE match_results (
    match_id TEXT NOT NULL PRIMARY KEY CHECK (length(match_id) > 0),
    winner TEXT NOT NULL CHECK (winner IN ('HEROES', 'DUNGEON_MASTER')),
    finished_at_utc TEXT NOT NULL
);

CREATE TABLE match_players (
    match_id TEXT NOT NULL REFERENCES match_results(match_id) ON DELETE CASCADE,
    player_id TEXT NOT NULL CHECK (length(player_id) > 0),
    display_name TEXT NOT NULL,
    role TEXT NOT NULL CHECK (length(role) > 0),
    PRIMARY KEY (match_id, player_id)
);

PRAGMA user_version = 1;

COMMIT;
