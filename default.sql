DROP TABLE IF EXISTS search;
CREATE TABLE search (hash TEXT, name TEXT, size INTEGER, server TEXT);
CREATE INDEX IF NOT EXISTS name_index ON search (name);
INSERT INTO search (hash, name, size, server)
VALUES ('6E0F7F9554A1DD9D5281776FD98B7E3E1B9F3098', 'Double Dragon.avi', '23842790', 'topaz,emerald,ruby');
INSERT INTO search (hash, name, size, server)
VALUES ('56F99BBDBFFB68C0FD63F665759F96B0BED2EE0E', 'Double Dragon 2.avi', '20576554', 'topaz,emerald,ruby');
INSERT INTO search (hash, name, size, server)
VALUES ('2DC4BEC5BC8A572C9CD315FDC66C4C34C68F7F3E', 'Excitebike.avi', '21953008', 'topaz,emerald,ruby');
INSERT INTO search (hash, name, size, server)
VALUES ('93ECA8B19E4EBF54C0E9C712B2F0E5742B3EBC0C', 'Super Mario 2 (Jap).avi', '21208890', 'topaz,emerald,ruby');
INSERT INTO search (hash, name, size, server)
VALUES ('E2F311E54882A423466B9C6F9F1F66C84B4B707A', 'Kings of Power 4 Billion %.avi', '337080320', 'topaz,emerald,ruby');
INSERT INTO search (hash, name, size, server)
VALUES ('75EC2B493DF470BDCA5592BF0FD45ECC20ED35A1', 'Primer.avi', '734001152', 'topaz,emerald,ruby');
