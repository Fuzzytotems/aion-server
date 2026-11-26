/*
 * Upstream sync 2026-11 (beyond-aion 4.8 51d7a19cb..68eccc7b1): the game server schema migration of a database at 51d7a19cb.
 *
 * REVIEWED, NOT APPLIED ANYWHERE. Written by lane B of the sync foundation (docs/design/upstream-sync-2026-11.md, "Foundation progress
 * (database)") for the cut-over of the play kit's live `aion_gs`. Back the database up first (mariadb-dump) and stop the game server.
 *
 * game-server/sql/update.sql at 68eccc7b1 only holds the delta since 620fc69 (the pet columns): upstream rebased its header twice in the
 * range and dropped the earlier statements, among them the inventory column of cc61fb239, which no later update.sql repeats. This file
 * collects every schema statement of the seven commits that change game-server/sql (`git log 51d7a19cb..68eccc7b1 -- game-server/sql`), in
 * commit order, as upstream wrote them. Section 1 is the schema the C++ DAOs of the sync foundation need; section 2 holds the data-only
 * statements, which belong to the behaviour of their wave and must not run before it is ported.
 *
 * Checked on 2026-11-25 (read only) against the play kit's `aion_gs`: it has the 51d7a19cb schema (house_scripts.script mediumtext and no
 * uncompressed_size, inventory without rank_limit_expire_time, player_appearance.jaw_height/chin_jut, player_pets with the mood columns and
 * despawn_time, player_recipes without production_limit, players without pvp_exp, player_effects.magical_criticals present) and 0
 * house_scripts rows. Section 1 was run on 2026-11-25 on throwaway schemas (aion_gs_test_sync_*, dropped afterwards): a copy of aion_gs.sql at
 * 51d7a19cb with a player, a house, two scripts (one empty) and an appearance row, migrated, had the same SHOW CREATE TABLE for all 61 tables
 * as a fresh aion_gs.sql at 68eccc7b1; the script became a 52-byte zlib block that inflates to its 88 UTF-16LE bytes, the empty one an empty
 * block of size 0, and jaw_height/chin_jut kept their values as chin_height/cheek_bones.
 */

-- ---------------------------------------------------------------------------------------------------------------------------------------
-- Section 1: schema (needed by the C++ game server once the sync foundation's DAOs are merged)
-- ---------------------------------------------------------------------------------------------------------------------------------------

-- cc61fb239 Item actions audit (#189): the 10-minute grace period of an item whose abyss rank requirement its owner lost
ALTER TABLE `inventory`
	ADD COLUMN `rank_limit_expire_time` int NOT NULL DEFAULT '0' AFTER `rnd_plume_bonus`;

-- d725054e1 Implement retail PvP EXP system and acquisition limits (#192)
ALTER TABLE `players`
	ADD COLUMN `pvp_exp` bigint NOT NULL DEFAULT '0' AFTER `reposte_energy`;

-- e5c133836 Fixed limited crafting recipes and morphing (#195): the remaining production count of a limited recipe. Existing rows keep 0,
-- which PlayerRecipesDAO.load replaces by the template's max_production_count
ALTER TABLE `player_recipes`
	ADD COLUMN `production_limit` tinyint NOT NULL DEFAULT '0' AFTER `recipe_id`;

-- a519ffc7b Clamped the character height and corrected the appearance fields: the client's names of the two sliders
ALTER TABLE `player_appearance`
	RENAME COLUMN `jaw_height` TO `chin_height`,
	RENAME COLUMN `chin_jut` TO `cheek_bones`;

-- 46e8d00b9 Fixed exploits of housing: scripts are stored as the client's zlib block (MariaDB's COMPRESS prefixes a 4-byte length, which
-- SUBSTRING(..., 5) drops) with the UTF-16LE size it inflates to. An empty script becomes an empty block of size 0
ALTER TABLE `house_scripts`
	ADD COLUMN `script_data` blob NULL AFTER `script_id`,
	ADD COLUMN `uncompressed_size` int NOT NULL DEFAULT '0' AFTER `script_data`;
UPDATE `house_scripts` SET
	`uncompressed_size` = LENGTH(CONVERT(`script` USING utf16le)),
	`script_data` = SUBSTRING(COMPRESS(CAST(CONVERT(`script` USING utf16le) AS BINARY)), 5);
ALTER TABLE `house_scripts`
	DROP COLUMN `script`,
	CHANGE COLUMN `script_data` `script` blob NOT NULL,
	ALTER COLUMN `uncompressed_size` DROP DEFAULT;

-- f16e03ad3 Retail teleport confirmation, pet rework and system message parameter order (#248): the pet's appearance; the mood is kept in
-- memory only and the last used pet is gone
ALTER TABLE `player_pets`
	ADD COLUMN `color` int NOT NULL DEFAULT '0' AFTER `decoration`,
	ADD COLUMN `face` int NOT NULL DEFAULT '0' AFTER `color`,
	ADD COLUMN `head` int NOT NULL DEFAULT '0' AFTER `face`;
ALTER TABLE `player_pets`
	DROP COLUMN `mood_started`,
	DROP COLUMN `counter`,
	DROP COLUMN `mood_cd_started`,
	DROP COLUMN `gift_cd_started`,
	DROP COLUMN `despawn_time`;

-- ---------------------------------------------------------------------------------------------------------------------------------------
-- Section 2: data, NOT part of the foundation. Run each statement only together with the port of its commit
-- ---------------------------------------------------------------------------------------------------------------------------------------

-- 267ce6033 Fixed skill grade being used as the skill level (#209, wave 3) and e5c133836 (#195, wave 5: morphing, skill 40009, no longer
-- gains levels): learned skills have level 1. The C++ server of 51d7a19cb still stores the grade as the level, so running this before wave 3
-- is ported would mix both kinds of rows (the play kit had 3256 such rows, levels up to 13, on 2026-11-25). The final form of e5c133836:
-- UPDATE `player_skills` SET `skill_level` = 1 WHERE (`skill_id` < 30000 OR `skill_id` = 40009);
