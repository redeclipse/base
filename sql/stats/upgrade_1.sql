/* DB Version 1 to Version 2 */
CREATE TABLE game_ffarounds (
    game INTEGER,
    player INTEGER,
    playerhandle TEXT,
    round INTEGER,
    winner BOOL
);

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
