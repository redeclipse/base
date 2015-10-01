/* DB Version 3 to Version 4 */
ALTER TABLE game_players ADD COLUMN timeactive INTEGER;
UPDATE game_players SET timeactive = timealive;
