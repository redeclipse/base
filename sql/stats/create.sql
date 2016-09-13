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
    game INTEGER,
    handle TEXT,
    flags TEXT,
    desc TEXT,
    version TEXT,
    host TEXT,
    port INTEGER
);

CREATE TABLE game_teams (
    game INTEGER,
    team INTEGER,
    score INTEGER,
    name TEXT
);

CREATE TABLE game_players (
    game INTEGER,
    name TEXT,
    handle TEXT,
    score INTEGER,
    timealive INTEGER,
    frags INTEGER,
    deaths INTEGER,
    wid INTEGER,
    timeactive INTEGER
);

CREATE TABLE game_weapons (
    game INTEGER,
    player INTEGER,
    playerhandle TEXT,
    weapon TEXT,

    timewielded INTEGER,
    timeloadout INTEGER,

    damage1 INTEGER,
    frags1 INTEGER,
    hits1 INTEGER,
    flakhits1 INTEGER,
    shots1 INTEGER,
    flakshots1 INTEGER,

    damage2 INTEGER,
    frags2 INTEGER,
    hits2 INTEGER,
    flakhits2 INTEGER,
    shots2 INTEGER,
    flakshots2 INTEGER
);

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
