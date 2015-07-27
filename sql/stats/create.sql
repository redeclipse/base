/*
	SQL Database Layout
*/

CREATE TABLE games (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	time INTEGER,
	map TEXT,
	mode INTEGER,
	mutators INTEGER,
	timeplayed INTEGER
);

CREATE TABLE game_servers (
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
	player TEXT,
	weapon TEXT,
	
	timewielded INTEGER,
	timeloadout INTEGER,
	
	damage1 INTEGER,
	kills1 INTEGER,
	hitratio1 REAL,
	
	damage2 INTEGER,
	kills2 INTEGER,
	hitratio2 REAL
);
