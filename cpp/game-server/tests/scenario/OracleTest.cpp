// The parsing of the spawn oracle's answers (m5a-plan.md F-05, §5.5), which the gate's V2 rules rest on. No server, no database and no Python
// interpreter: these are the two distinctions that made V2 assert the wrong thing on real data, pinned on hand written JSON.
//
//   - "level": 0 is a level, "level": null is "this spot has no npc template". tools/oracle/m5a/spawns.py:186 reads the npc_template attribute
//     with java_int(..., 0), so a template without a `level` attribute answers 0, and spawns.py:254 writes null for a gatherable spot. Reading
//     both as 0 made V2 skip the comparison for a genuine level-0 npc and assert `level > 0` on it instead - a failure on correct behaviour.
//   - Which ids may stand off their spots (isPinnedToFixedSpots). §5.5 V2 says an npc fails only when the oracle knows its id SOLELY as fixed
//     spots; V2 used to fail a walker-only or randomWalk-only id as well. A pool-only id stays pinned: its spots are real coordinates.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "Oracle.h"

namespace aion::gameserver::scenario {
namespace {

OracleSpot spotOf(int32_t npcId, bool pool, bool walker, bool randomWalk) {
	OracleSpot spot;
	spot.npcId = npcId;
	spot.pool = pool;
	spot.walker = walker;
	spot.randomWalk = randomWalk;
	return spot;
}

TEST(OracleTest, ALevelOfZeroIsALevelAndANullLevelIsNone) {
	const OracleSpot zero = Oracle::parseSpot(R"({"npcId": 1, "x": 1.0, "y": 2.0, "z": 3.0, "h": 4, "level": 0})");
	ASSERT_TRUE(zero.level.has_value()) << "an npc_template without a level attribute answers 0, which V2 has to compare";
	EXPECT_EQ(*zero.level, 0);

	const OracleSpot none = Oracle::parseSpot(R"({"npcId": 400601, "x": 1.0, "y": 2.0, "z": 3.0, "h": 4, "level": null})");
	EXPECT_FALSE(none.level.has_value()) << "a gatherable spot has no npc template, so the oracle reports null";

	const OracleSpot missing = Oracle::parseSpot(R"({"npcId": 2, "x": 1.0, "y": 2.0, "z": 3.0, "h": 4})");
	EXPECT_FALSE(missing.level.has_value()) << "an answer without the key at all is the same as null";

	const OracleSpot real = Oracle::parseSpot(R"({"npcId": 3, "x": 1.0, "y": 2.0, "z": 3.0, "h": 4, "level": 7})");
	ASSERT_TRUE(real.level.has_value());
	EXPECT_EQ(*real.level, 7);
}

TEST(OracleTest, TheRestOfASpotIsParsedAsTheOracleWritesIt) {
	const OracleSpot spot = Oracle::parseSpot(
	  R"({"npcId": 210115, "x": 1212.94, "y": 1044.85, "z": 140.76, "h": 60, "level": 3, "spawned": true, "deterministic": false,
	      "distance": 12.5, "flags": {"pool": true, "temporary": false, "walker": true, "randomWalk": false, "gatherable": false, "flag": false}})");
	EXPECT_EQ(spot.npcId, 210115);
	EXPECT_FLOAT_EQ(spot.x, 1212.94f);
	EXPECT_FLOAT_EQ(spot.y, 1044.85f);
	EXPECT_FLOAT_EQ(spot.z, 140.76f);
	EXPECT_EQ(spot.heading, 60);
	EXPECT_TRUE(spot.spawned);
	EXPECT_FALSE(spot.deterministic);
	EXPECT_DOUBLE_EQ(spot.distance, 12.5);
	EXPECT_TRUE(spot.pool);
	EXPECT_TRUE(spot.walker);
	EXPECT_FALSE(spot.randomWalk);
	EXPECT_FALSE(spot.gatherable);
	EXPECT_FALSE(spot.flag);
	EXPECT_FALSE(spot.temporary);
}

TEST(OracleTest, OnlyAnIdWhoseEverySpotIsFixedIsPinnedToItsCoordinates) {
	// §5.5 V2, spelled out per kind of id. The gate fails an npc for standing off its spots exactly when this is true.
	const std::vector<OracleSpot> spots = {
	  spotOf(100, false, false, false), // fixed only
	  spotOf(100, false, false, false),
	  spotOf(200, true, false, false), // pool only
	  spotOf(300, false, true, false), // walker only
	  spotOf(400, false, false, true), // randomWalk only
	  spotOf(500, false, false, false), // both: 210115's shape (fixed plus walker plus randomWalk)
	  spotOf(500, false, true, false),
	  spotOf(500, false, false, true),
	};
	EXPECT_TRUE(isPinnedToFixedSpots(spots, 100));
	EXPECT_TRUE(isPinnedToFixedSpots(spots, 200)) << "a pool npc stands on one of the pool coordinates the oracle lists for its id "
	                                               "(SpawnEngine.java:160-168), so V2 compares it against them like any fixed spot";
	EXPECT_FALSE(isPinnedToFixedSpots(spots, 300)) << "a walker group member moves with its formation";
	EXPECT_FALSE(isPinnedToFixedSpots(spots, 400)) << "a randomWalk npc wanders by definition";
	EXPECT_FALSE(isPinnedToFixedSpots(spots, 500)) << "an id with both fixed and moving spots may legitimately stand anywhere (§5.5 V2)";
	EXPECT_FALSE(isPinnedToFixedSpots(spots, 999)) << "an id the oracle does not know is V1's failure, not V2's";
	EXPECT_FALSE(isPinnedToFixedSpots({}, 100)) << "no spots at all pins nothing";
}

} // namespace
} // namespace aion::gameserver::scenario
