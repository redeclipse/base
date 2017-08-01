CREATE TABLE modes (
  id INTEGER PRIMARY KEY,
  name TEXT UNIQUE NOT NULL,
  longname TEXT UNIQUE NOT NULL,
  CHECK (name <> ''),
  CHECK (longname <> '')
);

INSERT INTO modes (id, name, longname) VALUES (1, 'bb', 'Bomber Ball');
INSERT INTO modes (id, name, longname) VALUES (2, 'ctf', 'Capture the Flag');
INSERT INTO modes (id, name, longname) VALUES (3, 'dac', 'Defend and Control');
INSERT INTO modes (id, name, longname) VALUES (4, 'demo', 'Demo');
INSERT INTO modes (id, name, longname) VALUES (5, 'dm', 'Deathmatch');
INSERT INTO modes (id, name, longname) VALUES (6, 'edit', 'Editing');
INSERT INTO modes (id, name, longname) VALUES (7, 'race', 'Race');

CREATE TABLE proto_modes (
  version INTEGER NOT NULL, -- RE protocol version
  value INTEGER NOT NULL, -- RE mode number
  mode_id INTEGER NOT NULL, -- DB mode id
  UNIQUE (version, value),
  UNIQUE (version, mode_id),
  FOREIGN KEY (mode_id) REFERENCES modes(id)
);

INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 0, 4); -- demo
INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 1, 6); -- edit
INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 2, 5); -- dm
INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 3, 2); -- ctf
INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 4, 3); -- dac
INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 5, 1); -- bb
INSERT INTO proto_modes (version, value, mode_id) VALUES (230, 6, 7); -- race

CREATE TABLE mutators (
  id INTEGER PRIMARY KEY,
  mode_id INTEGER,
  name TEXT NOT NULL,
  shortname TEXT NOT NULL,
  CHECK (name <> ''),
  CHECK (shortname <> ''),
  UNIQUE (mode_id, name),
  FOREIGN KEY (mode_id) REFERENCES modes(id)
);

-- base
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (1, NULL, 'basic', 'ba');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (2, NULL, 'classic', 'cl');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (3, NULL, 'coop', 'co');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (4, NULL, 'duel', 'du');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (5, NULL, 'ffa', 'ffa');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (6, NULL, 'freestyle', 'fr');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (7, NULL, 'hard', 'ha');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (8, NULL, 'insta', 'in');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (9, NULL, 'kaboom', 'ka');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (10, NULL, 'medieval', 'me');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (11, NULL, 'multi', 'mu');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (12, NULL, 'onslaught', 'on');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (13, NULL, 'resize', 're');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (14, NULL, 'survivor', 'su');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (15, NULL, 'vampire', 'va');
-- gsp
-- bb
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (16, 1, 'hold', 'ho');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (17, 1, 'basket', 'ba');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (18, 1, 'attack', 'at');
-- ctf
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (19, 2, 'quick', 'qu');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (20, 2, 'defend', 'de');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (21, 2, 'protect', 'pr');
-- dac
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (22, 3, 'quick', 'qu');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (23, 3, 'king', 'ki');
-- dm
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (24, 5, 'gladiator', 'gl');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (25, 5, 'oldschool', 'ol');
-- race
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (26, 7, 'timed', 'ti');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (27, 7, 'endurance', 'en');
INSERT INTO mutators (id, mode_id, name, shortname) VALUES (28, 7, 'gauntlet', 'ga');

CREATE TABLE proto_mutators (
  version INTEGER NOT NULL,
  mode INTEGER, -- RE mode number
  bit INTEGER NOT NULL, -- RE mutator bit
  mutator_id INTEGER NOT NULL, -- DB mutator id
  UNIQUE (version, mode, bit),
  UNIQUE (version, mode, mutator_id),
  FOREIGN KEY (mutator_id) REFERENCES mutators(id)
);

-- base
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 0, 11);  -- multi
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 1, 5);   -- ffa
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 2, 3);   -- coop
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 3, 8);   -- insta
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 4, 10);  -- medievel
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 5, 9);   -- kaboom
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 6, 4);   -- duel
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 7, 14);  -- survivor
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 8, 2);   -- classic
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 9, 12);  -- onslaught
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 10, 6);  -- freestyle
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 11, 15); -- vampire
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 12, 13); -- resize
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 13, 7);  -- hard
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, NULL, 14, 1);  -- basic
-- gsp
-- bb
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 5, 15, 16); -- hold
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 5, 16, 17); -- basket
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 5, 17, 18); -- attack
-- ctf
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 3, 15, 19); -- quick
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 3, 16, 20); -- defend
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 3, 17, 21); -- protect
-- dac
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 4, 15, 22); -- quick
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 4, 16, 23); -- king
-- dm
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 2, 15, 24); -- gladiator
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 2, 16, 25); -- oldschool
-- race
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 6, 15, 26); -- timed
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 6, 16, 27); -- endurance
INSERT INTO proto_mutators (version, mode, bit, mutator_id) VALUES (230, 6, 17, 28); -- gauntlet

ALTER TABLE games RENAME TO g_old;

CREATE TABLE games (
  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  time INTEGER NOT NULL,
  map TEXT NOT NULL,
  mode_id INTEGER NOT NULL,
  timeplayed INTEGER NOT NULL,
  uniqueplayers INTEGER NOT NULL,
  normalweapons INTEGER NOT NULL,
  CHECK (map <> ''),
  FOREIGN KEY (mode_id) REFERENCES modes(id)
);
CREATE INDEX games_map ON games(map);
CREATE INDEX games_mode_id ON games(mode_id);
CREATE INDEX games_normalweapons ON games(normalweapons);

INSERT INTO games (id, time, map, mode_id, timeplayed, uniqueplayers, normalweapons)
SELECT go.id, go.time, go.map, pm.mode_id, go.timeplayed, go.uniqueplayers, 1
FROM g_old AS go
JOIN proto_modes AS pm ON go.mode = pm.value;

CREATE TABLE game_mutators (
  game_id INTEGER NOT NULL,
  mutator_id INTEGER NOT NULL,
  UNIQUE (game_id, mutator_id),
  FOREIGN KEY (game_id) REFERENCES games(id),
  FOREIGN KEY (mutator_id) REFERENCES mutators(id)
);
CREATE INDEX game_mutators_game_id ON game_mutators(game_id);
CREATE INDEX game_mutators_mutator_id ON game_mutators(mutator_id);

INSERT INTO game_mutators (game_id, mutator_id)
SELECT go.id, m.mutator_id
FROM g_old AS go,
     proto_mutators AS m
WHERE (m.mode IS NULL OR m.mode = go.mode)
      AND (go.mutators & (1 << m.bit)) > 0;

DROP TABLE g_old;

UPDATE games SET normalweapons = 0
WHERE mode_id = 7 -- race
      OR id IN (SELECT DISTINCT game_id
                FROM game_mutators
                WHERE mutator_id IN (8, 9, 10, 24)); -- insta, kaboom, medieval, gladiator
