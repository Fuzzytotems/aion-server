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
	/**
	 * What SM_STATS_INFO writes as [base main hand attack] / [current main hand attack] with the enter-world passives applied
	 * (tools/oracle/m5a/creation.py stats_info_main_hand_p_attack, m5b2-plan.md §10.3 X1). std::nullopt where the oracle refuses to model it,
	 * with the reason in statsInfoNotModelled - never a 0, because a magical main hand legitimately answers 0 and 0.
	 */
	std::optional<int32_t> mainHandPAttackBase, mainHandPAttackCurrent;
	/**
	 * The CURRENT max HP and MP SM_STATS_INFO writes beside the base ones: the base plus the float bonus of the equipped items' bonus modifiers
	 * (creation.py stats_info_current_max) - the Mage's robe adds 47 MP. The bonus is what a later bonus function (the soul sickness, X10)
	 * adds to. std::nullopt where the oracle refuses to model it.
	 */
	std::optional<int32_t> maxHpCurrent, maxMpCurrent;
	std::optional<float> maxHpBonus, maxMpBonus;
	std::vector<std::string> statsInfoNotModelled;

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

/** One <change> of an effect template (BufEffect.getModifiers turns it into a stat function): stat, func (ADD, PERCENT, REPLACE) and value */
struct OracleStatChange {
	std::string stat, func;
	int32_t value = 0;
};

/** One effect template of an m5b2-skills entry, in the document order of <effects> */
struct OracleSkillEffect {
	/** the XML tag, e.g. "root" */
	std::string tag;
	/** the class Effects.java binds the tag to, e.g. "RootEffect", and its `extends` chain up to "EffectTemplate" */
	std::string effectClass;
	std::vector<std::string> classChain;
	/** the `e` attribute */
	int32_t position = 0;
	int32_t duration1 = 0, duration2 = 0, randomTime = 0;
	/** the <change> children, in document order (the soul sickness's MAXHP -30 PERCENT is what X10 derives the sickened max HP from) */
	std::vector<OracleStatChange> changes;
};

/** SkillTargetSlot of a skill template (tools/oracle/m5b2/skills.py) */
struct OracleTargetSlot {
	std::string name;
	/** what SM_ABNORMAL_STATE and SM_ABNORMAL_EFFECT write per effect */
	int32_t ordinal = 0;
	/** the slot mask of the packet-level slot byte */
	int32_t id = 0;
};

/**
 * One (skill id, level) entry of m5b2-skills (m5b2-plan.md G-01, D8): the template constants the M5b-2 gate asserts exactly. Every value the
 * oracle does not model is std::nullopt here - never a 0 - and the oracle says why in `notModelled`.
 */
struct OracleSkillTemplate {
	int32_t skillId = 0;
	/** the level of the cast / of the effect: the skill list level, the npc skill's lv, or the death count for the soul sickness */
	int32_t level = 0;
	/** "autolearn", "extra", "soulSickness", "npc:<id>" */
	std::vector<std::string> sources;
	std::string name, activation, method, subType, category;
	/** SkillTemplate.getLvl(), the level SM_CASTSPELL_RESULT writes */
	int32_t lvl = 0;
	/** std::nullopt for a template without a tslot */
	std::optional<OracleTargetSlot> targetSlot;
	int32_t baseCastDuration = 0;
	/** what SM_CASTSPELL writes for a player caster */
	std::optional<int32_t> castDuration;
	std::optional<float> castSpeed;
	bool allowAnimationBoost = false;
	/** SkillTemplate.getCooldownId(): the key of Player.skillCoolDowns and of the `player_cooldowns` table */
	int32_t cooldownId = 0;
	/** what SM_CASTSPELL_RESULT writes, in units of 100 ms */
	int32_t cooldown = 0;
	/** what SM_SKILL_COOLDOWN writes as the duration */
	int32_t cooldownMillis = 0;
	/** the END condition <mp> cost, 0 without one: the USED_MP of SM_ATTACK_STATUS */
	std::optional<int32_t> mpCost;
	std::optional<std::string> chainCategory;
	std::vector<OracleSkillEffect> effects;
	/** Effect.calculateTemplateDuration with every template successful, 0 for no timed effect */
	std::optional<int32_t> effectDuration;
	int32_t effectDurationRandomTime = 0;
	std::vector<std::string> notModelled;
};

/** One npc skill of m5b2-skills --npc */
struct OracleNpcSkill {
	int32_t skillId = 0, level = 0, prob = 0;
	bool isPostSpawn = false;
	/** Math.round(duration * cast_speed / 1000f), the npc's own cast bar */
	int32_t castDuration = 0;
};

/** One --npc of m5b2-skills */
struct OracleNpcSkills {
	int32_t npcId = 0, level = 0, castSpeed = 0;
	/** empty for an npc without an npc_skills list */
	std::vector<OracleNpcSkill> skills;
};

/** oracle.py m5b2-skills --race R --class C [--level N] [--skill ID[:LEVEL]] [--npc ID] [--death-count N] (m5b2-plan.md G-01) */
struct OracleSkills {
	std::string race, playerClass;
	int32_t level = 0;
	/** SkillLearnService.learnNewSkills(player, 1, level): what `player_skills` holds for a fresh character */
	std::vector<OracleSkill> characterSkills;
	std::vector<int32_t> passives;
	/** PlayerController.updateSoulSickness's skill (8291) and the death count it was reported at */
	int32_t soulSicknessSkillId = 0, deathCount = 0;
	std::vector<OracleNpcSkills> npcs;
	std::vector<OracleSkillTemplate> skills;
	/** the leaf effect classes of every reported skill, and their closure under `extends` (m5b2-plan.md §2.4) */
	std::vector<std::string> effectLeaves, effectClasses;

	/**
	 * The entry of `skillId` at `skillLevel`, or at its only level when `skillLevel` is not given.
	 * @throws std::out_of_range when the oracle reported no such entry, or several levels of the id and no level was given
	 */
	const OracleSkillTemplate& skill(int32_t skillId, std::optional<int32_t> skillLevel = std::nullopt) const;
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
	/**
	 * oracle.py m5b2-skills: the character's autolearn skills and the exact constants of every skill the gate casts or observes.
	 * @param extraSkills further skills as "ID" or "ID:LEVEL" - the ones the gate seeds into player_skills (plan D3)
	 * @param npcIds npcs whose npc_skills lists to report (X9's npc 210133)
	 * @param deathCount the level the soul sickness is reported at (the first death is 1)
	 */
	OracleSkills skills(std::string_view race, std::string_view playerClass, int32_t level = 1, const std::vector<std::string>& extraSkills = {},
		const std::vector<int32_t>& npcIds = {}, int32_t deathCount = 1) const;

	/**
	 * The command line skills() hands to oracle.py after the script path: the `m5b2-skills` sub command and one flag per parameter, spelled as
	 * tools/oracle/oracle.py's argparse spells them (`--race`, `--class`, `--level`, `--death-count`, one `--skill` per extra skill and one `--npc`
	 * per npc id). Public so that OracleTest pins the binding without a Python interpreter; OracleRunTest drives it through the real oracle.
	 */
	static std::vector<std::string> skillsArguments(std::string_view race, std::string_view playerClass, int32_t level,
		const std::vector<std::string>& extraSkills, const std::vector<int32_t>& npcIds, int32_t deathCount);

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

	/**
	 * Parses a whole m5b2-skills answer. Public like parseMonster: the nulls the oracle writes for what it does not model (castDuration,
	 * castSpeed, mpCost, effectDuration, targetSlot, chainCategory) must stay distinguishable from a 0, and OracleTest pins that without a server
	 * (@throws on invalid JSON).
	 */
	static OracleSkills parseSkills(std::string_view skillsJson);

private:
	std::filesystem::path python;
	std::filesystem::path script;
	std::filesystem::path workDir;
	mutable int32_t runCounter = 0;
};

} // namespace aion::gameserver::scenario
