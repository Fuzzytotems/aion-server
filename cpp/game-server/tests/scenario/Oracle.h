#pragma once

// The independent oracles of the M5a scenario gate (m5a-plan.md F-05, §5.3 "DB checks against m5a-creation", §5.5, §5.6): tools/oracle/oracle.py
// run as a child process, its JSON answer parsed here. The oracle reads the Java static data (game-server/data) with its own Python code, so an
// assertion against it never shares an implementation with the C++ server. Without a Python interpreter (AION_TEST_PYTHON) the gate is skipped.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
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
	int32_t level = 0;
	/** false for a spot that the given game time does not spawn */
	bool spawned = false;
	/** a spot whose object is at a known position: not in a pool, not a walker and not randomly walking */
	bool deterministic = false;
	bool pool = false, temporary = false, walker = false, randomWalk = false, gatherable = false, flag = false;
	double distance = 0;
};

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

private:
	std::filesystem::path python;
	std::filesystem::path script;
	std::filesystem::path workDir;
	mutable int32_t runCounter = 0;
};

} // namespace aion::gameserver::scenario
