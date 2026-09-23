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
#include <stdexcept>
#include <string>
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

// m5b-monster (m5b-plan.md G-01), pinned the same way: the fields the M5b gate reads out of the answer, and the two of them the oracle may
// write as null - `nearestPlainSpot` when no spot of the id is plain (every one has a static id, is pooled or walks), and `respawnTime` when
// the id's spawn groups do not share one. Both must stay distinguishable from a 0, which is why they are not read with value().
constexpr const char* MONSTER_ANSWER = R"({
  "format": "aion-m5b-monster", "version": 1, "map": 210010000, "npcId": 210663, "playerLevel": 1,
  "template": {"level": 2, "maxHp": 199, "rating": "NORMAL", "rank": "DISCIPLINED", "rankOrdinal": 1, "aggroRange": 8, "aggroAngle": 270,
               "attackRange": 2, "attackSpeed": 2142, "race": "BEAST", "tribe": "MONSTER", "ai": "aggressive",
               "boundRadius": {"front": 0.55, "side": 0.56, "upper": 2.82, "maxOfFrontAndSide": 0.56}},
  "spots": [
    {"x": 1210.58, "y": 1083.4, "z": 138.75, "h": 12, "staticId": 4, "ai": "aggressive", "respawnTime": 20, "spawned": true, "fixed": true,
     "distance": 38.673},
    {"x": 1193.01, "y": 1087.08, "z": 137.559, "h": 56, "staticId": 3, "ai": null, "respawnTime": 20, "spawned": null, "fixed": false,
     "distance": 46.805},
    {"x": 1226.22, "y": 1096.57, "z": 141.93, "h": 2, "staticId": 0, "ai": "aggressive", "respawnTime": 20, "spawned": true, "fixed": true,
     "distance": 53.408}],
  "pinned": true,
  "nearestPlainSpot": {"x": 1226.22, "y": 1096.57, "z": 141.93, "h": 2, "staticId": 0, "ai": "aggressive", "respawnTime": 20, "spawned": true,
                       "fixed": true, "distance": 53.408},
  "respawnTime": 20, "respawnTimes": [20],
  "exp": {"ratingMultiplier": 2.4000001, "baseExp": 478, "expMultiplier": 1.25, "xpPercentage": 105, "experienceReward": 627,
          "xpSoloRate": 1.0, "expNeed": 400, "cap": 80.0, "awarded": 80},
  "player": {"race": "ELYOS", "playerClass": "WARRIOR", "attackRangeStat": 1500, "attackSpeed": 1400, "movementSpeed": 6000},
  "ranges": {"attackRange": 3.31, "maxCoveredDistance": 0.6, "toleranceRange": 3.9099998}
})";

TEST(OracleTest, MonsterAnswerIsParsedAsTheOracleWritesIt) {
	const OracleMonster monster = Oracle::parseMonster(MONSTER_ANSWER);
	EXPECT_EQ(monster.mapId, 210010000);
	EXPECT_EQ(monster.npcId, 210663);
	EXPECT_EQ(monster.playerLevel, 1);
	EXPECT_EQ(monster.level, 2);
	EXPECT_EQ(monster.maxHp, 199);
	EXPECT_EQ(monster.rating, "NORMAL");
	EXPECT_EQ(monster.rank, "DISCIPLINED");
	EXPECT_EQ(monster.race, "BEAST");
	EXPECT_EQ(monster.tribe, "MONSTER");
	EXPECT_EQ(monster.ai, "aggressive");
	EXPECT_EQ(monster.aggroRange, 8);
	EXPECT_EQ(monster.aggroAngle, 270);
	EXPECT_EQ(monster.npcAttackRange, 2);
	EXPECT_EQ(monster.npcAttackSpeed, 2142);
	EXPECT_FLOAT_EQ(monster.boundRadius, 0.56f);

	ASSERT_EQ(monster.spots.size(), 3u);
	EXPECT_EQ(monster.spots[0].staticId, 4) << "the nearest spot of all carries a static id, which is why the gate may not take it";
	EXPECT_TRUE(monster.spots[0].fixed);
	EXPECT_TRUE(monster.spots[0].spawned);
	EXPECT_DOUBLE_EQ(monster.spots[0].distance, 38.673);
	EXPECT_EQ(monster.spots[1].ai, "") << "a spot of an npc template without an ai name";
	EXPECT_FALSE(monster.spots[1].spawned) << "`spawned: null` (a pool or an unknown game time) is not a spawned spot";
	EXPECT_FALSE(monster.spots[1].fixed);
	EXPECT_TRUE(monster.pinned);
	EXPECT_EQ(monster.respawnTime, 20);

	ASSERT_TRUE(monster.nearestPlainSpot.has_value());
	EXPECT_EQ(monster.nearestPlainSpot->staticId, 0);
	EXPECT_FLOAT_EQ(monster.nearestPlainSpot->x, 1226.22f);
	EXPECT_FLOAT_EQ(monster.nearestPlainSpot->y, 1096.57f);
	EXPECT_FLOAT_EQ(monster.nearestPlainSpot->z, 141.93f);
	EXPECT_EQ(monster.nearestPlainSpot->heading, 2);
	EXPECT_EQ(monster.nearestPlainSpot->respawnTime, 20);
	EXPECT_EQ(monster.nearestPlainSpot->ai, "aggressive");

	EXPECT_EQ(monster.baseExp, 478);
	EXPECT_EQ(monster.xpPercentage, 105);
	EXPECT_EQ(monster.experienceReward, 627);
	EXPECT_EQ(monster.expNeed, 400);
	EXPECT_EQ(monster.awarded, 80) << "the Rates.XP_HUNTING cap, not the reward, is what the character gains";
	EXPECT_FLOAT_EQ(monster.attackRange, 3.31f);
	EXPECT_FLOAT_EQ(monster.toleranceRange, 3.9099998f);
	EXPECT_FLOAT_EQ(monster.maxCoveredDistance, 0.6f);
	EXPECT_EQ(monster.playerAttackSpeed, 1400);
}

TEST(OracleTest, AMonsterWithoutAPlainSpotHasNoNearestPlainSpot) {
	const OracleMonster monster = Oracle::parseMonster(R"({
	  "map": 1, "npcId": 2, "playerLevel": 1,
	  "template": {"level": 1, "maxHp": 10, "rating": "JUNK", "rank": "NOVICE", "boundRadius": {"maxOfFrontAndSide": 0.0}},
	  "spots": [{"x": 1.0, "y": 2.0, "z": 3.0, "h": 4, "staticId": 7, "respawnTime": 0, "spawned": true, "fixed": true, "distance": 5.0}],
	  "pinned": true, "nearestPlainSpot": null, "respawnTime": null, "respawnTimes": [0, 20],
	  "exp": {"baseExp": 22, "xpPercentage": 100, "experienceReward": 28, "expNeed": 400, "awarded": 28},
	  "player": {"attackSpeed": 1400}, "ranges": {"attackRange": 1.0, "maxCoveredDistance": 0.6, "toleranceRange": 1.6}})");
	EXPECT_FALSE(monster.nearestPlainSpot.has_value()) << "every spot of this id carries a static id: the gate has no spot to fight at";
	EXPECT_EQ(monster.respawnTime, 0) << "a null respawn time (the id's groups disagree) reads as 0, and 0 is 'no respawn' either way";
	EXPECT_EQ(monster.spots.size(), 1u);
	EXPECT_EQ(monster.spots[0].ai, "") << "no ai name at all";
	EXPECT_EQ(monster.tribe, "") << "a template without a tribe";
}

// m5b2-skills (m5b2-plan.md G-01): the constants the M5b-2 gate asserts exactly. The answer below is a real run of
// `oracle.py m5b2-skills --race ELYOS --class MAGE --npc 210133`, trimmed to the gate's three Mage ids (1282, 1328, 8291) and to the fields
// Oracle::parseSkills reads, so the parser is pinned to the format the oracle actually writes.
constexpr const char* SKILLS_ANSWER = R"({"format": "aion-m5b2-skills", "version": 1, "race": "ELYOS", "playerClass": "MAGE", "level": 1,
  "character": {"skills": [{"skillId": 40, "level": 1}, {"skillId": 100, "level": 1}, {"skillId": 103, "level": 1}, {"skillId": 243, "level": 1},
                           {"skillId": 245, "level": 1}, {"skillId": 302, "level": 1}, {"skillId": 1282, "level": 1}, {"skillId": 1328, "level": 1},
                           {"skillId": 30001, "level": 1}], "passives": [40, 100, 103]},
  "soulSickness": {"skillId": 8291, "deathCount": 1, "maxDeathCount": 10},
  "npcs": [{"npcId": 210133, "name": "striped kerub", "level": 1, "castSpeed": 1000, "skills": [{"skillId": 16419, "level": 1, "prob": 25,
            "minHp": 0, "maxHp": 100, "cd": 0, "prio": 0, "nextSkillTime": -1, "isPostSpawn": false, "target": "MOST_HATED", "castDuration": 2500}]}],
  "skills": [
    {"skillId": 1282, "level": 1, "sources": ["autolearn"], "name": "Flame Bolt", "lvl": 1, "skillType": "MAGICAL", "subType": "ATTACK",
     "category": "CHAIN_SKILL", "activation": "ACTIVE", "method": "CAST", "targetSlot": {"name": "NONE", "ordinal": 7, "id": 128},
     "baseCastDuration": 2000, "castDuration": 2000, "castSpeed": 1.0, "allowAnimationBoost": true, "cooldown": 0, "cooldownMillis": 0,
     "chainCategory": "M_CHAINA_1TH_1", "mpCost": 19, "effects": [{"tag": "spellatkinstant", "class": "SpellAttackInstantEffect",
     "classChain": ["SpellAttackInstantEffect", "DamageEffect", "EffectTemplate"], "position": 1, "duration1": 0, "duration2": 0,
     "randomTime": 0}], "effectDuration": 0, "effectDurationRandomTime": 0, "notModelled": []},
    {"skillId": 1328, "level": 1, "sources": ["autolearn"], "name": "Root", "lvl": 1, "skillType": "MAGICAL", "subType": "DEBUFF",
     "category": "PHYSICAL_DEBUFF", "activation": "ACTIVE", "method": "CAST", "targetSlot": {"name": "DEBUFF", "ordinal": 1, "id": 2},
     "baseCastDuration": 0, "castDuration": 0, "castSpeed": 1.0, "allowAnimationBoost": true, "cooldown": 600, "cooldownMillis": 60000,
     "chainCategory": null, "mpCost": 38, "effects": [{"tag": "root", "class": "RootEffect", "classChain": ["RootEffect", "EffectTemplate"],
     "position": 1, "duration1": 0, "duration2": 20000, "randomTime": 0}], "effectDuration": 20000, "effectDurationRandomTime": 0, "notModelled": []},
    {"skillId": 8291, "level": 1, "sources": ["soulSickness"], "name": "Soul Sickness", "lvl": 1, "skillType": "MAGICAL", "subType": "NONE",
     "category": "NONE", "activation": "PROVOKED", "method": "PROVOKED", "targetSlot": {"name": "SPEC2", "ordinal": 4, "id": 16},
     "baseCastDuration": 0, "castDuration": 0, "castSpeed": 1.0, "allowAnimationBoost": false, "cooldown": 0, "cooldownMillis": 0,
     "chainCategory": null, "mpCost": 0, "effects": [{"tag": "statdown", "class": "StatdownEffect", "classChain": ["StatdownEffect", "BufEffect",
     "EffectTemplate"], "position": 1, "duration1": 20000, "duration2": 40000, "randomTime": 0}], "effectDuration": 60000,
     "effectDurationRandomTime": 0, "notModelled": []}],
  "effectClasses": {"leaves": ["AlwaysDodgeEffect", "AlwaysResistEffect", "ArmorMasteryEffect", "EscapeEffect", "HealInstantEffect", "ReturnEffect",
                               "RootEffect", "SkillAttackInstantEffect", "SpellAttackInstantEffect", "StatdownEffect", "WeaponMasteryEffect"],
                    "withBases": ["AbstractHealEffect", "AlwaysDodgeEffect", "AlwaysResistEffect", "ArmorMasteryEffect", "BufEffect", "DamageEffect",
                                  "EffectTemplate", "EscapeEffect", "HealInstantEffect", "ReturnEffect", "RootEffect", "SkillAttackInstantEffect",
                                  "SpellAttackInstantEffect", "StatdownEffect", "WeaponMasteryEffect"]}})";

TEST(OracleTest, SkillsAnswerIsParsedAsTheOracleWritesIt) {
	const OracleSkills skills = Oracle::parseSkills(SKILLS_ANSWER);
	EXPECT_EQ(skills.race, "ELYOS");
	EXPECT_EQ(skills.playerClass, "MAGE");
	EXPECT_EQ(skills.level, 1);
	ASSERT_EQ(skills.characterSkills.size(), 9u);
	EXPECT_EQ(skills.characterSkills[3].skillId, 243) << "the class-less Return";
	EXPECT_EQ(skills.characterSkills[6].skillId, 1282);
	EXPECT_EQ(skills.characterSkills[6].level, 1);
	EXPECT_EQ(skills.passives, (std::vector<int32_t>{40, 100, 103}));
	EXPECT_EQ(skills.soulSicknessSkillId, 8291);
	EXPECT_EQ(skills.deathCount, 1);
	ASSERT_EQ(skills.npcs.size(), 1u);
	EXPECT_EQ(skills.npcs[0].npcId, 210133);
	EXPECT_EQ(skills.npcs[0].level, 1);
	EXPECT_EQ(skills.npcs[0].castSpeed, 1000);
	ASSERT_EQ(skills.npcs[0].skills.size(), 1u);
	EXPECT_EQ(skills.npcs[0].skills[0].skillId, 16419);
	EXPECT_EQ(skills.npcs[0].skills[0].level, 1);
	EXPECT_EQ(skills.npcs[0].skills[0].prob, 25);
	EXPECT_FALSE(skills.npcs[0].skills[0].isPostSpawn);
	EXPECT_EQ(skills.npcs[0].skills[0].castDuration, 2500);
	EXPECT_EQ(skills.effectLeaves.size(), 11u);
	EXPECT_EQ(skills.effectClasses.size(), 15u);
	EXPECT_EQ(skills.effectClasses[6], "EffectTemplate");

	const OracleSkillTemplate& bolt = skills.skill(1282);
	EXPECT_EQ(bolt.name, "Flame Bolt");
	EXPECT_EQ(bolt.sources, (std::vector<std::string>{"autolearn"}));
	EXPECT_EQ(bolt.castDuration, 2000) << "X4's cast bar";
	EXPECT_EQ(bolt.castSpeed, 1.0f);
	EXPECT_TRUE(bolt.allowAnimationBoost);
	EXPECT_EQ(bolt.mpCost, 19) << "X4's MP";
	EXPECT_EQ(bolt.cooldown, 0);
	EXPECT_EQ(bolt.baseCastDuration, 2000);
	EXPECT_EQ(bolt.chainCategory, "M_CHAINA_1TH_1");
	ASSERT_TRUE(bolt.targetSlot.has_value());
	EXPECT_EQ(bolt.targetSlot->name, "NONE");
	ASSERT_EQ(bolt.effects.size(), 1u);
	EXPECT_EQ(bolt.effects[0].effectClass, "SpellAttackInstantEffect");
	EXPECT_EQ(bolt.effects[0].classChain, (std::vector<std::string>{"SpellAttackInstantEffect", "DamageEffect", "EffectTemplate"}));
	EXPECT_EQ(bolt.effectDuration, 0) << "0 is 'no timed effect', which is an answer, not a missing one";

	const OracleSkillTemplate& root = skills.skill(1328, 1);
	ASSERT_TRUE(root.targetSlot.has_value());
	EXPECT_EQ(root.targetSlot->ordinal, 1) << "what SM_ABNORMAL_EFFECT writes per effect for X6";
	EXPECT_EQ(root.targetSlot->id, 2) << "the slot mask";
	EXPECT_EQ(root.effectDuration, 20000) << "X6's 20-second debuff";
	EXPECT_EQ(root.cooldown, 600);
	EXPECT_EQ(root.cooldownMillis, 60000);
	EXPECT_EQ(root.mpCost, 38);
	EXPECT_FALSE(root.chainCategory.has_value()) << "null, not an empty category";
	EXPECT_EQ(root.effects[0].tag, "root");
	EXPECT_EQ(root.effects[0].duration2, 20000);
	EXPECT_EQ(root.effects[0].duration1, 0);

	const OracleSkillTemplate& sickness = skills.skill(skills.soulSicknessSkillId);
	EXPECT_EQ(sickness.activation, "PROVOKED");
	EXPECT_EQ(sickness.method, "PROVOKED");
	EXPECT_EQ(sickness.targetSlot->ordinal, 4) << "SPEC2";
	EXPECT_EQ(sickness.targetSlot->id, 16);
	EXPECT_EQ(sickness.effectDuration, 60000) << "X10: 40000 + 20000 * deathCount 1";
	EXPECT_EQ(sickness.effects[0].duration1, 20000);
	EXPECT_EQ(sickness.effects[0].duration2, 40000);
	EXPECT_EQ(sickness.lvl, 1);
}

TEST(OracleTest, WhatTheSkillsOracleDoesNotModelStaysNull) {
	// tools/oracle/m5b2/skills.py writes null (with a `notModelled` reason) for a value it refuses to guess, and 0 where 0 is the answer
	const OracleSkills skills = Oracle::parseSkills(R"({"race": "ELYOS", "playerClass": "WARRIOR", "level": 3,
	  "character": {"skills": [], "passives": []}, "soulSickness": {"skillId": 8291, "deathCount": 2}, "npcs": [{"npcId": 5, "skills": []}],
	  "skills": [
	    {"skillId": 7, "level": 1, "targetSlot": null, "castDuration": null, "castSpeed": null, "mpCost": null, "chainCategory": null,
	     "effects": [], "effectDuration": null, "effectDurationRandomTime": null, "notModelled": ["castDuration: charge", "mpCost: ratio"]},
	    {"skillId": 7, "level": 3, "targetSlot": {"name": "BUFF", "ordinal": 0, "id": 1}, "castDuration": 0, "castSpeed": 0.0, "mpCost": 0,
	     "effects": [], "effectDuration": 0, "effectDurationRandomTime": 250, "notModelled": []}],
	  "effectClasses": {"leaves": [], "withBases": []}})");
	EXPECT_EQ(skills.level, 3);
	EXPECT_EQ(skills.deathCount, 2);
	ASSERT_EQ(skills.npcs.size(), 1u);
	EXPECT_TRUE(skills.npcs[0].skills.empty()) << "an npc without a list";

	const OracleSkillTemplate& unknown = skills.skill(7, 1);
	EXPECT_FALSE(unknown.castDuration.has_value());
	EXPECT_FALSE(unknown.castSpeed.has_value());
	EXPECT_FALSE(unknown.mpCost.has_value());
	EXPECT_FALSE(unknown.effectDuration.has_value());
	EXPECT_FALSE(unknown.targetSlot.has_value());
	EXPECT_FALSE(unknown.chainCategory.has_value());
	EXPECT_EQ(unknown.effectDurationRandomTime, 0);
	EXPECT_EQ(unknown.notModelled, (std::vector<std::string>{"castDuration: charge", "mpCost: ratio"}));

	const OracleSkillTemplate& zeros = skills.skill(7, 3);
	EXPECT_EQ(zeros.castDuration, 0) << "a 0 is engaged, not null";
	EXPECT_EQ(zeros.castSpeed, 0.0f);
	EXPECT_EQ(zeros.mpCost, 0);
	EXPECT_EQ(zeros.effectDuration, 0);
	EXPECT_EQ(zeros.effectDurationRandomTime, 250);
	ASSERT_TRUE(zeros.targetSlot.has_value());
	EXPECT_EQ(zeros.targetSlot->ordinal, 0) << "BUFF's ordinal is 0 and its id 1";
	EXPECT_EQ(zeros.targetSlot->id, 1);
}

TEST(OracleTest, ASkillOfSeveralLevelsNeedsItsLevel) {
	const OracleSkills skills = Oracle::parseSkills(R"({"race": "ELYOS", "playerClass": "MAGE", "level": 1,
	  "character": {"skills": [], "passives": []}, "soulSickness": {"skillId": 8291, "deathCount": 1}, "npcs": [],
	  "skills": [{"skillId": 8291, "level": 1, "effects": [], "effectDuration": 60000},
	             {"skillId": 8291, "level": 3, "effects": [], "effectDuration": 100000},
	             {"skillId": 1282, "level": 1, "effects": []}],
	  "effectClasses": {"leaves": [], "withBases": []}})");
	EXPECT_EQ(skills.skill(8291, 1).effectDuration, 60000);
	EXPECT_EQ(skills.skill(8291, 3).effectDuration, 100000) << "the level picks the entry";
	EXPECT_THROW(skills.skill(8291), std::out_of_range) << "two levels of the id and no level given";
	EXPECT_EQ(skills.skill(1282).skillId, 1282) << "an id of one level needs none";
	EXPECT_THROW(skills.skill(1282, 2), std::out_of_range);
	EXPECT_THROW(skills.skill(4242), std::out_of_range);
}

} // namespace
} // namespace aion::gameserver::scenario
