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
);
