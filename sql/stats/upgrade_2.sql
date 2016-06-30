/* DB Version 2 to Version 3 */
ALTER TABLE games ADD COLUMN uniqueplayers INTEGER DEFAULT 0;
