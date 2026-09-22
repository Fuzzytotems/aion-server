#pragma once

// The independent oracles of the M5a scenario gate (m5a-plan.md F-05, §5.3 "DB checks against m5a-creation", §5.5, §5.6): tools/oracle/oracle.py
// run as a child process, its JSON answer parsed here. The oracle reads the Java static data (game-server/data) with its own Python code, so an
// assertion against it never shares an implementation with the C++ server. Without a Python interpreter (AION_TEST_PYTHON) the gate is skipped.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::scenario {

/** One item of m5a-creation */
struct OracleItem {
	int32_t itemId = 0;
	int64_t count = 0;
	bool kinah = false;
	bool equipped = false;
	/** the equipment slot mask (0 for an item that is not equipped) */
	int64_t slot = 0;
};

/** One skill of m5a-creation */
struct OracleSkill {
	int32_t skillId = 0;
	int32_t level = 0;
};

/** oracle.py m5a-creation --race R --class C */
struct OracleCreation {
	int32_t mapId = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
	std::vector<OracleItem> items;
	std::vector<OracleSkill> skills;
	int32_t baseMaxHp = 0, baseMaxMp = 0;

	/** the items without the kinah row, i.e. what the `inventory` table holds besides kinah */
	std::vector<OracleItem> equippedItems() const;
};

/** One spawn spot of m5a-spawns */
struct OracleSpot {
	int32_t npcId = 0;
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
	/**
	 * The `level` attribute of the npc_template of this id, std::nullopt when the oracle has no npc template for the spot at all (a gatherable
	 * spot, whose template is a GatherableTemplate). It is an optional and not a 0 default because **0 is a legitimate level**: the oracle reads
	 * `java_int(element.get("level"), ..., 0)` (tools/oracle/m5a/spawns.py:186), so an npc_template without a `level` attribute answers 0, and
	 * `"level": null` (spawns.py:254, `template.level if template else None`) means something else entirely. With a plain int both collapsed into
	 * 0 and V2 read a genuine level-0 npc as "the oracle has no level for this one", i.e. it compared nothing and then asserted level > 0, which
	 * fails on the very npc it was supposed to check.
	 */
	std::optional<int32_t> level;
	/** false for a spot that the given game time does not spawn */
	bool spawned = false;
	/** a spot whose object is at a known position: not in a pool, not a walker and not randomly walking */
	bool deterministic = false;
	bool pool = false, temporary = false, walker = false, randomWalk = false, gatherable = false, flag = false;
	double distance = 0;
};

/**
 * m5a-plan.md §5.5 V2: true when the oracle pins `npcId` to fixed coordinates - it knows at least one spot of that id and **none** of them is a
 * pool, walker or randomWalk spot. Only such an npc fails V2 for standing somewhere else; an id with a pool, walker or randomWalk spot may
 * legitimately stand anywhere and is covered by V1 (id plus oracle distance) alone, "such as 210115 on the Elyos start map (4 fixed, 3 walker,
 * 1 randomWalk)".
 *
 * @param spots every spot of the answer, not only those of `npcId`
 */
bool isPinnedToFixedSpots(const std::vector<OracleSpot>& spots, int32_t npcId);

/** oracle.py m5a-spawns --map M --x X --y Y --z Z [--game-hour H] */
struct OracleSpawns {
	int32_t mapId = 0;
	double radius = 0;
	std::vector<OracleSpot> spots;
	/** the flag npcs of the map (V1 accepts them at any distance) */
	std::vector<OracleSpot> flagNpcs;
};

/** oracle.py m5a-border-target */
struct OracleBorderTarget {
	int32_t mapId = 0;
	float startX = 0, startY = 0, startZ = 0;
	float targetX = 0, targetY = 0, targetZ = 0;
	double distance = 0;
	/** npcs that are within the visibility radius of the target but not of the start */
	std::vector<OracleSpot> appear;
	/** npcs that are within the visibility radius of the start but not of the target */
	std::vector<OracleSpot> disappear;
};

/** One spawn spot of m5b-monster (tools/oracle/m5b/monster.py): every field the gate needs to pick and to recognize its monster */
struct OracleMonsterSpot {
	float x = 0, y = 0, z = 0;
	int32_t heading = 0;
	/**
	 * SpawnSpotTemplate.staticId, 0 for a plain spot. **The gate takes a spot whose static id is 0** (m5b-plan.md D11): a static id makes
	 * `Npc::hasStatic()` true, which answers `ask(IS_IMMUNE_TO_ABNORMAL_STATES)` true and puts GeoService's placeable object in and out of the
	 * spawn and death paths - couplings a combat gate did not ask for.
	 */
	int32_t staticId = 0;
	/** Spawn.respawnTime in seconds; 0 means the spot never respawns (SpawnTemplate.isNoRespawn) */
	int32_t respawnTime = 0;
	/** the ai name this spot's npc gets: the spot's own `ai` attribute if it has one, else the template's (Creature.java:64-66) */
	std::string ai;
	/** false for a spot the given game time does not spawn */
	bool spawned = false;
	/** not in a pool, not a walker and not randomly walking, i.e. an npc that stands exactly here */
	bool fixed = false;
	/** metres from the race's spawn point */
	double distance = 0;
};

/** oracle.py m5b-monster --map M --npc-id N [--player-level L] (m5b-plan.md G-01) */
struct OracleMonster {
	int32_t mapId = 0, npcId = 0, playerLevel = 0;

	// the npc template (NpcTemplate.java), with its JAXB defaults
	int32_t level = 0, maxHp = 0;
	std::string rating, rank, race, tribe, ai;
	int32_t aggroRange = 0, aggroAngle = 0, npcAttackRange = 0, npcAttackSpeed = 0;
	/** BoundRadius.getMaxOfFrontAndSide, which is what PositionUtil.isInRange adds to a range */
	float boundRadius = 0;

	/** every regular spawn spot of the id on the map, nearest first */
	std::vector<OracleMonsterSpot> spots;
	/** every spot is `fixed`: only then may an assertion pin the npc to an exact position (the V2 rule of m5a-plan.md §5.5) */
	bool pinned = false;
	/** the nearest spot that is `fixed`, spawned and has no static id - the spot the gate fights at */
	std::optional<OracleMonsterSpot> nearestPlainSpot;
	/** the respawn time in seconds when every spot of the id shares one, else 0 */
	int32_t respawnTime = 0;

	// the experience of one kill (m5b-plan.md D7)
	int32_t baseExp = 0, xpPercentage = 0;
	int64_t experienceReward = 0, expNeed = 0;
	/** what STR_GET_EXP carries and what `players.exp` grows by: `(long) min(reward * rate, expNeed * 0.2f)` */
	int64_t awarded = 0;

	// the two ranges of the first hit (PlayerController.java:400-409)
	float attackRange = 0, toleranceRange = 0, maxCoveredDistance = 0;
	/** PlayerGameStats.getAttackSpeed of the fresh character, i.e. the pace fightUntil needs */
	int32_t playerAttackSpeed = 0;
};

/** Runs tools/oracle/oracle.py. Every call starts a process and parses its stdout as JSON. */
class Oracle {
public:
	/**
	 * @param python the interpreter (AION_TEST_PYTHON), @param oracleScript tools/oracle/oracle.py,
	 * @param workDir a directory for the captured stdout and stderr of the runs
	 */
	Oracle(std::filesystem::path python, std::filesystem::path oracleScript, std::filesystem::path workDir);

	/** @return the oracle of the gate, std::nullopt if AION_TEST_PYTHON is unset or the script is missing (the gate is then skipped) */
	static std::optional<Oracle> fromEnvironment(const std::filesystem::path& workDir);

	/** @throws std::runtime_error if the oracle fails or writes no JSON (its stderr is part of the message) */
	OracleCreation creation(std::string_view race, std::string_view playerClass) const;
	/** @param radius the query radius in metres (V1 accepts a walker up to 105 m away, so the gate asks for more than the default 100) */
	OracleSpawns spawns(int32_t mapId, float x, float y, float z, int32_t gameHour, double radius = 120.0) const;
	OracleBorderTarget borderTarget(int32_t mapId, float x, float y, float z, int32_t gameHour) const;
	/** @param playerLevel the level of the character that gets the kill, which decides the XPRewardEnum percentage and the experience cap */
	OracleMonster monster(int32_t mapId, int32_t npcId, int32_t playerLevel) const;

	/** the raw JSON text of a run, for a decoder of its own */
	std::string run(const std::vector<std::string>& arguments) const;

	/**
	 * Parses one `spots` entry of an m5a-spawns answer. Public because the gate's V2 rules depend on the difference between "level 0" and "no
	 * level" and between a fixed and a moving spot, which OracleTest pins without a server (@throws on invalid JSON).
	 */
	static OracleSpot parseSpot(std::string_view spotJson);

	/**
	 * Parses a whole m5b-monster answer. Public for the same reason as parseSpot: the rules the gate depends on - which spot is the plain one,
	 * that a missing `nearestPlainSpot` is null rather than a zeroed spot, that `respawnTime` may be null when the id's groups disagree - are
	 * pinned by OracleTest without a server (@throws on invalid JSON).
	 */
	static OracleMonster parseMonster(std::string_view monsterJson);

private:
	std::filesystem::path python;
	std::filesystem::path script;
	std::filesystem::path workDir;
	mutable int32_t runCounter = 0;
};

} // namespace aion::gameserver::scenario
