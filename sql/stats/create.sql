/*
	SQL Database Layout
*/

CREATE TABLE games (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	time INTEGER,
	map TEXT,
	mode INTEGER,
	mutators INTEGER,
	server INTEGER,
	timeplayed INTEGER,
);

CREATE TABLE game_servers (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
    game INTEGER,
    handle TEXT,
    desc TEXT,
    version TEXT,
    ip TEXT,
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
    timeplayed INTEGER
);

CREATE TABLE game_weapons (
	game INTEGER,
	id TEXT,
	damage INTEGER,
	kills INTEGER,
	hitratio REAL,
	timewielded INTEGER,
	timeloadout INTEGER
);
