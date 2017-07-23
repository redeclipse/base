/*
    SQL Database Layout
*/

CREATE TABLE games (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    time INTEGER,
    map TEXT,
    mode INTEGER,
    mutators INTEGER,
    timeplayed INTEGER,
    uniqueplayers INTEGER,
    usetotals INTEGER
);

CREATE TABLE game_servers (
    game INTEGER PRIMARY KEY,
    handle TEXT NOT NULL,
    flags TEXT NOT NULL,
    desc TEXT NOT NULL,
    version TEXT NOT NULL,
    host TEXT NOT NULL,
    port INTEGER NOT NULL,
    CHECK (handle <> ''),
    FOREIGN KEY (game) REFERENCES games(id)
);
CREATE INDEX game_servers_handle ON game_servers(handle);

CREATE TABLE game_teams (
    game INTEGER,
    team INTEGER,
    score INTEGER,
    name TEXT
);

CREATE TABLE game_players (
    game INTEGER NOT NULL,
    name TEXT NOT NULL,
    handle TEXT,
    score INTEGER NOT NULL,
    timealive INTEGER NOT NULL,
    timeactive INTEGER NOT NULL,
    frags INTEGER NOT NULL,
    deaths INTEGER NOT NULL,
    wid INTEGER NOT NULL,
    CHECK (name <> ''),
    CHECK (handle <> ''),
    UNIQUE (game, wid),
    UNIQUE (game, handle), -- nulls are distinct from each other
    FOREIGN KEY (game) REFERENCES games(id)
);
CREATE INDEX game_players_game ON game_players(game);
CREATE INDEX game_players_handle ON game_players(handle);

CREATE TABLE game_weapons (
    game INTEGER NOT NULL,
    player INTEGER NOT NULL,
    playerhandle TEXT,
    weapon TEXT NOT NULL,

    timewielded INTEGER NOT NULL,
    timeloadout INTEGER NOT NULL,

    damage1 INTEGER NOT NULL,
    frags1 INTEGER NOT NULL,
    hits1 INTEGER NOT NULL,
    flakhits1 INTEGER NOT NULL,
    shots1 INTEGER NOT NULL,
    flakshots1 INTEGER NOT NULL,

    damage2 INTEGER NOT NULL,
    frags2 INTEGER NOT NULL,
    hits2 INTEGER NOT NULL,
    flakhits2 INTEGER NOT NULL,
    shots2 INTEGER NOT NULL,
    flakshots2 INTEGER NOT NULL,

    CHECK (timewielded > 0 OR timeloadout > 0),
    CHECK (playerhandle <> ''),
    CHECK (weapon IN ('melee', 'pistol', 'sword', 'shotgun', 'smg', 'flamer', 'plasma', 'zapper', 'rifle', 'grenade', 'mine', 'rocket', 'claw')),
    FOREIGN KEY (game) REFERENCES games(id)
    FOREIGN KEY (game, player) REFERENCES game_players(game, wid)
);
CREATE INDEX game_weapons_game ON game_weapons(game);
CREATE INDEX game_weapons_playerhandle ON game_weapons(playerhandle);
CREATE INDEX game_weapons_weapon ON game_weapons(weapon);

CREATE TABLE game_ffarounds (
    game INTEGER,
    player INTEGER,
    playerhandle TEXT,
    round INTEGER,
    winner BOOL
);

/* Affinities */

CREATE TABLE game_captures (
    game INTEGER,
    player INTEGER,
    playerhandle TEXT,
    capturing INTEGER,
    captured INTEGER
);

CREATE TABLE game_bombings (
    game INTEGER,
    player INTEGER,
    playerhandle TEXT,
    bombing INTEGER,
    bombed INTEGER
);
