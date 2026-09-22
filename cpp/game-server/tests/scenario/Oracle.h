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

	/** the raw JSON text of a run, for a decoder of its own */
	std::string run(const std::vector<std::string>& arguments) const;

	/**
	 * Parses one `spots` entry of an m5a-spawns answer. Public because the gate's V2 rules depend on the difference between "level 0" and "no
	 * level" and between a fixed and a moving spot, which OracleTest pins without a server (@throws on invalid JSON).
	 */
	static OracleSpot parseSpot(std::string_view spotJson);

private:
	std::filesystem::path python;
	std::filesystem::path script;
	std::filesystem::path workDir;
	mutable int32_t runCounter = 0;
};

} // namespace aion::gameserver::scenario
