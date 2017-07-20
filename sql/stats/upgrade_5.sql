BEGIN EXCLUSIVE;

-- game weapons

DELETE FROM game_weapons WHERE
  weapon NOT IN ('melee', 'pistol', 'sword', 'shotgun', 'smg', 'flamer', 'plasma', 'zapper', 'rifle', 'grenade', 'mine', 'rocket', 'claw')
  OR timewielded = 0 AND timeloadout = 0
  OR game NOT IN (SELECT id FROM games);

UPDATE game_weapons SET playerhandle = NULL WHERE playerhandle = '';

ALTER TABLE game_weapons RENAME TO gw_old;

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

INSERT OR ROLLBACK INTO game_weapons (game, player, playerhandle, weapon, timewielded, timeloadout, damage1, frags1, hits1, flakhits1, shots1, flakshots1, damage2, frags2, hits2, flakhits2, shots2, flakshots2)
SELECT game, player, playerhandle, weapon, timewielded, timeloadout, damage1, frags1, hits1, flakhits1, shots1, flakshots1, damage2, frags2, hits2, flakhits2, shots2, flakshots2
FROM gw_old;

DROP TABLE gw_old;

-- game players

DELETE FROM game_players WHERE timealive = 0 AND timeactive = 0 OR name = '' OR name IS NULL;
DELETE FROM games WHERE id IN (SELECT game FROM game_players GROUP BY game, wid HAVING COUNT(*) > 1);
DELETE FROM game_players WHERE game NOT IN (SELECT id FROM games);
DELETE FROM game_weapons WHERE game NOT IN (SELECT id FROM games);
DELETE FROM game_weapons WHERE ROWID IN (SELECT gw.ROWID from game_weapons as gw left join game_players as gp on gp.game=gw.game and gp.wid=gw.player WHERE name IS NULL);

UPDATE game_players SET handle = NULL WHERE handle = '';

ALTER TABLE game_players RENAME TO gp_old;

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

INSERT OR ROLLBACK INTO game_players (game, name, handle, score, timealive, timeactive, frags, deaths, wid)
SELECT gpo.game, gpo.name, gpo.handle, gpo.score, gpo.timealive, gpo.timeactive, gpo.frags, gpo.deaths, gpo.wid
FROM gp_old AS gpo
JOIN (SELECT DISTINCT game AS gw_game, player AS gw_wid FROM game_weapons) ON gw_game = gpo.game AND gw_wid = gpo.wid;

DROP TABLE gp_old;

-- game servers

DELETE FROM game_servers WHERE handle = '' OR handle IS NULL;
DELETE FROM games WHERE id NOT IN (SELECT game FROM game_servers);
DELETE FROM game_servers WHERE game NOT IN (SELECT id FROM games);
DELETE FROM game_weapons WHERE game NOT IN (SELECT id FROM games);
DELETE FROM game_players WHERE game NOT IN (SELECT id FROM games);

ALTER TABLE game_servers RENAME TO gs_old;

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

INSERT OR ROLLBACK INTO game_servers (game, handle, flags, desc, version, host, port)
SELECT game, handle, flags, desc, version, host, port
FROM gs_old;

DROP TABLE gs_old;

DELETE FROM games WHERE id IN (SELECT g.id from games as g left join game_players as gp on g.id=gp.game WHERE name IS NULL);

COMMIT;
