#pragma once

// Test support of the cast lane (P5-02a, M5b-2 stage 1 part 2, item S-08): casts driven end to end against a real Player in a real map instance,
// on a DeterministicExecutor whose ManualClock the test moves, with every packet the Player is sent captured by a real AionConnection.
//
// - The Player, its connection and the packet capture are tests/cm_ak/InWorldPacketRunSupport.h's (makePlayer, TestClient), included by
//   relative path the way tests/cm_lz already includes it: the two chunks share no test support directory, and copying 400 lines of fixture
//   would fork it. That Player is a level-1 Warrior with the real PlayerGameStats and PlayerLifeStats (maxHp 244, maxMp 210, run speed 6 m/s),
//   so an MP cost is paid out of a real life-stat container.
// - The map: Poeta as one WorldMap2DInstance with a GeneralInstanceHandler, because Skill.endCast ends with
//   `effector.getWorldMapInstance().getInstanceHandler().onEndCastSkill(this)` (Skill.java:699) - a player outside a map instance would end every
//   cast in a NullPointerException. The holders a region reads are published once per process (ZoneService and regionSize cache theirs), as
//   tests/controllers/AttackSeamTest.cpp does.
// - SKILL_DATA holds the test skill templates below, bound from XML text through the real binder; MOTION_DATA the motion table
//   calculateAnimationTimesAfterLastHit reads after every player cast, with one motion, "testmotion" (a template without <motion> has no
//   motion time: Java null).
// - An npc target (CastTestNpc): the real Npc constructor with stat doubles, as tests/ai/AiTestSupport.h builds it, a MONSTER of level 1
//   (tribe_relations: MONSTER is hostile to PC, so Player.isEnemy(npc) is true), with the effect controller and known list the spawner adds.
//
// Templates without an <effects> element never create an Effect (Skill.endCast checks `skillTemplate.getEffects() != null`), which is how
// these tests reach every body of the cast machine without the effect leaf classes of part 3. The one template WITH an effect (EFFECT_SKILL,
// a <skillatk>) exists to find where a cast first meets an unported body - see SkillCastTest.TheEffectsAreCreatedAfterTheCastEnded.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../cm_ak/InWorldPacketRunSupport.h"

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/MotionData.bind.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SkillChargeData.bind.h"
#include "aion/gameserver/dataholders/SkillChargeData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::test {

namespace cp = network::aion::clientpackets::testing;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/** A 2,000 ms self cast that costs 19 MP, the shape of 1282 Flame Bolt without its target and its effect (skill_templates.xml) */
inline constexpr int32_t TIMED_SKILL = 60001;
/** An instant self skill with a 10-second cooldown (cooldown="100", tenths of a second) and no cost */
inline constexpr int32_t INSTANT_SKILL = 60002;
/**
 * The first skill of a chain: 2864 Ferocious Strike's <chain category="W_CHAINA_1TH_1"/> start condition, chain_skill_prob="100" and
 * cooldown="100" (ChainCondition.shouldReset keeps a first skill without `time` active for cooldown * 100 = 10 s after its last use)
 */
inline constexpr int32_t CHAIN_SKILL = 60003;
/** 1328 Root's properties: first_target TARGET, first_target_range 25, target_relation ENEMY, target_type ONLYONE; 38 MP */
inline constexpr int32_t ENEMY_SKILL = 60004;
/** 2864's <skillatk> effect on a self skill: the one template here that creates an Effect */
inline constexpr int32_t EFFECT_SKILL = 60005;
/** A PROVOKED skill (the shape of 8291 Soul Sickness, which no player learns) */
inline constexpr int32_t PROVOKED_SKILL = 60006;
/** A passive skill without effects (SkillEngine.applyEffectDirectly of enter-world) */
inline constexpr int32_t PASSIVE_SKILL = 60007;
/** The second skill of the chain opened by CHAIN_SKILL (precategory, precount 1) */
inline constexpr int32_t CHAIN_FOLLOWUP_SKILL = 60008;
/** A 1,000 ms self cast costing HP: 10 + 2 x skill level */
inline constexpr int32_t HP_COST_SKILL = 60009;
/** An instant self skill with a <motion> (motion_times "testmotion" of CAST_TEST_MOTION_TIMES_XML below) */
inline constexpr int32_t MOTION_SKILL = 60010;
/** An MP cost with a per-level delta: <mp value="20" delta="5"/> */
inline constexpr int32_t MP_DELTA_SKILL = 60011;
/** An MP cost as a percentage of max MP: <mp value="10" ratio="true"/> */
inline constexpr int32_t MP_RATIO_SKILL = 60012;
/** An enemy AREA skill around the first target: effective_range 10, target_maxcount 2 */
inline constexpr int32_t AREA_SKILL = 60013;
/** 1838 Healing Light's properties: first_target TARGETORME, target_relation FRIEND */
inline constexpr int32_t HEAL_SKILL = 60014;
/** A bag of start conditions the condition cases call one by one: weapon SWORD, target NPC, combatcheck, move_casting allow=false */
inline constexpr int32_t CONDITIONS_SKILL = 60015;
/** The two skills of charge 1 (CAST_TEST_SKILL_CHARGE_XML) and the CHARGE skill that starts it (<skillcharge value="1"/>) */
inline constexpr int32_t CHARGED_SKILL_1 = 60016;
inline constexpr int32_t CHARGED_SKILL_2 = 60017;
inline constexpr int32_t CHARGE_START_SKILL = 60018;
/** AREA_SKILL without a target_maxcount: every enemy within effective_range 10 of the first target */
inline constexpr int32_t AREA_ALL_SKILL = 60019;
/** 1282 Flame Bolt's shape without its effect and cost: a 2,000 ms cast at an enemy TARGET within 25 m (startCast's DeathObserver) */
inline constexpr int32_t TIMED_ENEMY_SKILL = 60020;
/** An <actions> cost next to an end condition: <mp value="10"/> and <mpuse value="7" delta="1"/><dpuse value="50"/> */
inline constexpr int32_t ACTIONS_SKILL = 60021;
/**
 * 1282 Flame Bolt without its chain, costs and effects: a 2,000 ms MAGICAL ATTACK at an enemy TARGET within 25 m, ammospeed="30",
 * apply_casting_time_bonus="true", <useconditions><move_casting allow="false"/></useconditions> and <motion name="pointfire"/>
 */
inline constexpr int32_t FLAME_BOLT_SKILL = 60022;
/** CHARGE_START_SKILL with apply_casting_time_bonus="true", so charge 1's MAGICAL charge_time_bonus_type reads the cast speed */
inline constexpr int32_t CHARGE_BOOSTED_SKILL = 60023;
/** An instant self skill with cooldown="100" and cooldown_delta_lv="-6" (the -6 of 219 templates of skill_templates.xml) */
inline constexpr int32_t COOLDOWN_DELTA_SKILL = 60024;
/** The first skill of a second chain: selfcount="2", no time, cooldown="10" - ChainCondition.shouldReset's window is cooldown * 100 = 1 s */
inline constexpr int32_t CHAIN_WINDOW_SKILL = 60025;
/** The follow-up of CHAIN_WINDOW_SKILL's chain that needs two activations of it: precategory, precount="2" */
inline constexpr int32_t CHAIN_PRECOUNT_SKILL = 60026;
/** An instant self skill with <motion name="testmotion"/> and apply_casting_time_bonus="true": endCast sets the hit-time boost */
inline constexpr int32_t BOOSTED_MOTION_SKILL = 60027;
/** An ATTACK with apply_casting_time_bonus="true" and a 2,000 ms cast (the BOOST_CASTING_TIME_ATTACK arm of getSkillCastBoostStat) */
inline constexpr int32_t BOOSTED_ATTACK_SKILL = 60028;

/** The skill_data document the fixture publishes: the templates above, in the attribute spelling of skill_templates.xml */
inline std::string castTestSkillData() {
	return R"(<skill_data>)"
		R"(<skill_template skill_id="60001" name="timed" nameId="1" cooldownId="501" stack="T1" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="2000" apply_casting_time_bonus="true">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="19" delta="0"/></endconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60002" name="instant" nameId="1" cooldownId="502" stack="T2" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="100" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60003" name="chain" nameId="1" cooldownId="503" stack="T3" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( skill_category="CHAIN_SKILL" chain_skill_prob="100" activation="ACTIVE" cooldown="100" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<startconditions><chain category="T_CHAINA_1TH_1"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60004" name="enemy" nameId="1" cooldownId="504" stack="T4" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
		R"( activation="ACTIVE" cooldown="600" duration="0">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="38" delta="0"/></endconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60005" name="effect" nameId="1" cooldownId="505" stack="T5" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="7" delta="0"/></endconditions>)"
		R"(<effects><skillatk value="27" e="1" accmod2="0" hoptype="DAMAGE"/></effects>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60006" name="provoked" nameId="1" stack="T6" lvl="1" skilltype="PHYSICAL" skillsubtype="DEBUFF")"
		R"( activation="PROVOKED" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60007" name="passive" nameId="1" stack="T7" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="PASSIVE" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60008" name="followup" nameId="1" cooldownId="508" stack="T8" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( skill_category="CHAIN_SKILL" chain_skill_prob="100" activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<startconditions><chain category="T_CHAINA_2TH_1" precategory="T_CHAINA_1TH_1" time="5000"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60009" name="hpcost" nameId="1" cooldownId="509" stack="T9" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="1000">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<endconditions><hp value="10" delta="2"/></endconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60010" name="motion" nameId="1" cooldownId="510" stack="T10" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<motion name="testmotion"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60011" name="mpdelta" nameId="1" cooldownId="511" stack="T11" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="20" delta="5"/></endconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60012" name="mpratio" nameId="1" cooldownId="512" stack="T12" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="10" ratio="true"/></endconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60013" name="area" nameId="1" cooldownId="513" stack="T13" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="AREA" effective_range="10")"
		R"( target_maxcount="2"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60014" name="heal" nameId="1" cooldownId="514" stack="T14" lvl="1" skilltype="MAGICAL" skillsubtype="HEAL" tslot="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="TARGETORME" first_target_range="25" target_relation="FRIEND" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60015" name="conditions" nameId="1" cooldownId="515" stack="T15" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE"/>)"
		R"(<startconditions><weapon weapon="SWORD MACE"/><target value="NPC"/><combatcheck/><move_casting allow="false"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60016" name="charged1" nameId="1" cooldownId="516" stack="T16" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60017" name="charged2" nameId="1" cooldownId="517" stack="T17" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<motion name="testmotion"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60018" name="chargestart" nameId="1" cooldownId="518" stack="T18" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="CHARGE" cooldown="0" duration="1900">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<startconditions><skillcharge value="1"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60019" name="areaall" nameId="1" cooldownId="519" stack="T19" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="AREA" effective_range="10"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60020" name="timedenemy" nameId="1" cooldownId="520" stack="T20" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( activation="ACTIVE" cooldown="0" duration="2000">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60021" name="actions" nameId="1" cooldownId="521" stack="T21" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="10" delta="0"/></endconditions>)"
		R"(<actions><mpuse value="7" delta="1"/><dpuse value="50"/></actions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60022" name="flamebolt" nameId="1" cooldownId="522" stack="T22" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( activation="ACTIVE" cooldown="0" duration="2000" ammospeed="30" apply_casting_time_bonus="true">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" revision_distance="12"/>)"
		R"(<useconditions><move_casting allow="false"/></useconditions>)"
		R"(<motion name="pointfire"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60023" name="chargeboosted" nameId="1" cooldownId="523" stack="T23" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( activation="CHARGE" cooldown="0" duration="1900" apply_casting_time_bonus="true">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<startconditions><skillcharge value="1"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60024" name="cooldowndelta" nameId="1" cooldownId="524" stack="T24" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="100" cooldown_delta_lv="-6" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60025" name="chainwindow" nameId="1" cooldownId="525" stack="T25" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( skill_category="CHAIN_SKILL" chain_skill_prob="100" activation="ACTIVE" cooldown="10" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<startconditions><chain category="T_CHAINB_1TH_1" selfcount="2"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60026" name="chainprecount" nameId="1" cooldownId="526" stack="T26" lvl="1" skilltype="PHYSICAL" skillsubtype="BUFF")"
		R"( skill_category="CHAIN_SKILL" chain_skill_prob="100" activation="ACTIVE" cooldown="0" duration="0">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<startconditions><chain category="T_CHAINB_2TH_1" precategory="T_CHAINB_1TH_1" time="5000" precount="2"/></startconditions>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60027" name="boostedmotion" nameId="1" cooldownId="527" stack="T27" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0" apply_casting_time_bonus="true">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(<motion name="testmotion"/>)"
		R"(</skill_template>)"
		R"(<skill_template skill_id="60028" name="boostedattack" nameId="1" cooldownId="528" stack="T28" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( activation="ACTIVE" cooldown="0" duration="2000" apply_casting_time_bonus="true">)"
		R"(<properties first_target="ME" target_type="ONLYONE"/>)"
		R"(</skill_template>)"
		R"(</skill_data>)";
}

/** skill_charge.xml reduced to one charge of two steps (the shape of charge 1: min_time 400, 1,600 ms per step) */
inline const char* const CAST_TEST_SKILL_CHARGE_XML = R"(<skill_charge><charge min_time="400" id="1" charge_time_bonus_type="MAGICAL">)"
													  R"(<skill time="1600" id="60016"/><skill time="1600" id="60017"/>)"
													  R"(</charge></skill_charge>)";

/**
 * motion_times.xml with two motions, Elyos male without a weapon: "testmotion" with the two motion ids a two-step charge skill reads, and
 * 1282 Flame Bolt's "pointfire" as motion_times.xml has it (min = max 0.4, animation_length 1.0)
 */
inline const char* const CAST_TEST_MOTION_TIMES_XML = R"(<motion_times><motion_time name="testmotion">)"
													  R"(<elyos_male weapon="noweapon" id="1" min="0.2" max="0.5" animation_length="1.0"/>)"
													  R"(<elyos_male weapon="noweapon" id="2" min="0.3" max="0.8" animation_length="1.5"/>)"
													  R"(</motion_time><motion_time name="pointfire">)"
													  R"(<elyos_male weapon="noweapon" id="1" min="0.4" max="0.4" animation_length="1.0"/>)"
													  R"(</motion_time></motion_times>)";

// ------------------------------------------------------------------------------------------------------------------------- static data

inline const char* const CAST_TEST_WORLD_MAPS_XML = R"(<world_maps>)"
													R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
													R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
													R"(</world_maps>)";

/** tribe_relations.xml reduced to what Npc.isEnemyFrom(Player) asks: a MONSTER is hostile to the two player tribes */
inline const char* const CAST_TEST_TRIBE_RELATIONS_XML = R"(<tribe_relations>)"
														 R"(<tribe name="PC"/><tribe name="PC_DARK"/>)"
														 R"(<tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile></tribe>)"
														 R"(</tribe_relations>)";

/**
 * The holders a map region reads, published once per process and never reset (ZoneService and WorldMapInstance::regionSize cache theirs; the
 * pattern of tests/controllers/AttackSeamTest.cpp). MATERIAL_DATA is TargetRelationProperty's (`isMaterialSkill`, TargetRelationProperty.java:22).
 * The unit tests load no geo data: with gameserver.geodata.cansee.enable off, GeoService.canSee answers true, which FirstTargetRangeProperty asks.
 */
inline void publishCastStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(
			xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), CAST_TEST_WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

inline std::vector<runtime::Ref<gameserver::model::gameobjects::player::PetCommonData>> castTestNoPets(gameserver::model::gameobjects::player::Player&) {
	return {};
}

// ------------------------------------------------------------------------------------------------------------------------- npc

/**
 * The HP the next CastTestNpc starts with: 1000, or 0 for CastTest::spawnCorpse - a dead npc without the death handling that reducing its HP to
 * 0 would run (CreatureLifeStats.onHpChanged -> NpcController.onDie). Set and read on the test thread only.
 */
inline int32_t castTestNpcStartHp = 1000;

/** Life stats with fixed HP and MP (NpcLifeStats reads the stat calculation; the npc's HP is not what these tests measure) */
class CastTestNpcLifeStats final : public gameserver::model::stats::container::CreatureLifeStats {
public:
	explicit CastTestNpcLifeStats(gameserver::model::gameobjects::Creature& owner) : CreatureLifeStats(owner, castTestNpcStartHp, 100) {}
};

/** An Npc with the real constructor and postConstruct chain; only the stat containers are doubles (tests/ai/AiTestSupport.h) */
class CastTestNpc final : public gameserver::model::gameobjects::Npc {
	AION_MAKE_REF_FRIEND
public:
	CastTestNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, gameserver::model::templates::spawns::SpawnTemplate& spawnTemplate,
		const gameserver::model::templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~CastTestNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<gameserver::model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<CastTestNpcLifeStats>(*this));
	}
};

class CastTestSpawnTemplate final : public gameserver::model::templates::spawns::SpawnTemplate {
public:
	explicit CastTestSpawnTemplate(gameserver::model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 100.0f, 100.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Npc templates are immortal static data, like the holder's own; `stats` is the <stats> element (NpcGameStats reads maxHp/maxMp from it) */
inline const gameserver::model::templates::npc::NpcTemplate* castTestNpcTemplate(std::string_view attributes, std::string_view stats) {
	xml::LoadContext context;
	return xml::bindString<gameserver::model::templates::npc::NpcTemplate>(context,
		"<npc_template name_id=\"1\" " + std::string(attributes) + ">" + std::string(stats) + "</npc_template>")
		.release();
}

// ------------------------------------------------------------------------------------------------------------------------- packets

/** The opcode of one captured packet: the header is [H encodeServerPacketOpcode(opcode)][C 0x44][H ~encoded] (AionServerPacket::writeOP) */
template <class P>
bool isPacket(const std::vector<uint8_t>& bytes) {
	if (bytes.size() < 2)
		return false;
	int32_t encoded = static_cast<int32_t>(static_cast<uint16_t>(bytes[0] | bytes[1] << 8));
	return encoded == (network::Crypt::encodeServerPacketOpcode(network::aion::opcodeOf<P>) & 0xFFFF);
}

/** The captured packets of type P, in the order they were queued */
template <class P>
std::vector<std::vector<uint8_t>> packetsOf(const std::vector<std::vector<uint8_t>>& sent) {
	std::vector<std::vector<uint8_t>> result;
	for (const std::vector<uint8_t>& bytes : sent)
		if (isPacket<P>(bytes))
			result.push_back(bytes);
	return result;
}

/** SM_CASTSPELL's fields, decoded from its bytes as SM_CASTSPELL.java:46-76 writes them (targetType 0/3/4: an object id) */
struct CastSpellFields {
	int32_t effectorId = 0;
	int32_t spellId = 0;
	int32_t level = 0;
	int32_t targetType = 0;
	int32_t targetObjectId = 0;
	int32_t castDuration = 0;
};

inline CastSpellFields decodeCastSpell(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	CastSpellFields f;
	f.effectorId = reader.D();
	f.spellId = static_cast<uint16_t>(reader.H());
	f.level = reader.C();
	f.targetType = reader.C();
	if (f.targetType == 0 || f.targetType == 3 || f.targetType == 4)
		f.targetObjectId = reader.D();
	f.castDuration = static_cast<uint16_t>(reader.H());
	return f;
}

/** SM_CASTSPELL_RESULT's leading fields (SM_CASTSPELL_RESULT.java:51-...): effector, targetType, target, skill, level, cooldown, hitTime, the status */
struct CastSpellResultFields {
	int32_t effectorId = 0;
	int32_t targetType = 0;
	int32_t targetObjectId = 0;
	int32_t skillId = 0;
	int32_t level = 0;
	int32_t cooldown = 0;
	int32_t hitTime = 0;
	int32_t status = 0;
	int32_t effectCount = 0;
};

inline CastSpellResultFields decodeCastSpellResult(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	CastSpellResultFields f;
	f.effectorId = reader.D();
	f.targetType = reader.C();
	if (f.targetType == 0 || f.targetType == 3 || f.targetType == 4)
		f.targetObjectId = reader.D();
	f.skillId = static_cast<uint16_t>(reader.H());
	f.level = reader.C();
	f.cooldown = reader.D();
	f.hitTime = static_cast<uint16_t>(reader.H());
	reader.C(); // unk
	f.status = reader.C();
	reader.C(); // 0, or 4 for a penalty skill
	int32_t dashStatus = reader.C();
	if (dashStatus == 1 || dashStatus == 2 || dashStatus == 3 || dashStatus == 4 || dashStatus == 6)
		reader.B(13);
	f.effectCount = static_cast<uint16_t>(reader.H());
	return f;
}

// ------------------------------------------------------------------------------------------------------------------------- stats and configs

/**
 * Adds `value` to one stat of the creature as a bonus, through a stat function without an owner (tests/effects_al/EffectTemplateTest.cpp's
 * addStat; RoahCustomInstanceHandler adds its functions the same way). A ReverseStat subtracts a bonus (ReverseStat.addToBonus), so +300 on
 * BOOST_CASTING_TIME shortens a 2,000 ms cast to 1,700 ms, and an AdditionStat adds it (Skill.updateCastDurationAndSpeed reads getBonus()).
 */
inline void addStat(gameserver::model::gameobjects::Creature& creature, gameserver::model::stats::container::StatEnum stat, int32_t value) {
	namespace functions = gameserver::model::stats::calc::functions;
	creature.getGameStats()->addEffect(nullptr,
		{runtime::Ptr<functions::IStatFunction>(functions::RcStatFunction<functions::StatAddFunction>::create(stat, value, true))});
}

/** Sets an atomic config value (GSConfig, SecurityConfig, ...) for one test and restores the value it had before */
template <class T>
class ConfigOverride {
public:
	ConfigOverride(std::atomic<T>& config, T value) : config(config), previous(config.load()) { config.store(value); }
	~ConfigOverride() { config.store(previous); }
	ConfigOverride(const ConfigOverride&) = delete;
	ConfigOverride& operator=(const ConfigOverride&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

// ------------------------------------------------------------------------------------------------------------------------- fixture

class CastTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 23);
		executor = backend.get(); // owned by ThreadPoolManager until TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		publishCastStaticDataOnce();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&castTestNoPets);
		world::knownlist::KnownList::resetNotifyFailureCountForTests();
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));

		xml::LoadContext context;
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, castTestSkillData()));
		dataholders::DataManager::MOTION_DATA.publish(xml::bindString<dataholders::MotionData>(context, CAST_TEST_MOTION_TIMES_XML));
		dataholders::DataManager::SKILL_CHARGE_DATA.publish(xml::bindString<dataholders::SkillChargeData>(context, CAST_TEST_SKILL_CHARGE_XML));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, cp::PLAYER_EXPERIENCE_TABLE_XML));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, CAST_TEST_TRIBE_RELATIONS_XML));

		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(210010000));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<instance::handlers::InstanceHandler>(instance::handlers::GeneralInstanceHandler::create(instance));
		});
		group = gameserver::model::templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr);
		spawnTemplate = runtime::Ref<CastTestSpawnTemplate>(
			static_cast<CastTestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<CastTestSpawnTemplate>(*group))));

		caster = cp::makePlayer(410001, 9401, "Caster");
		place(*caster.player, 100.0f, 100.0f, 50.0f);
		client = std::make_unique<cp::TestClient>();
		client->enterWorld(caster);
		learn({TIMED_SKILL, INSTANT_SKILL, CHAIN_SKILL, ENEMY_SKILL, EFFECT_SKILL, CHAIN_FOLLOWUP_SKILL, HP_COST_SKILL, MOTION_SKILL, MP_DELTA_SKILL,
			MP_RATIO_SKILL, AREA_SKILL, HEAL_SKILL, CONDITIONS_SKILL, CHARGED_SKILL_1, CHARGED_SKILL_2, CHARGE_START_SKILL,
			AREA_ALL_SKILL, TIMED_ENEMY_SKILL, ACTIONS_SKILL, FLAME_BOLT_SKILL, CHARGE_BOOSTED_SKILL, COOLDOWN_DELTA_SKILL, CHAIN_WINDOW_SKILL,
			CHAIN_PRECOUNT_SKILL, BOOSTED_MOTION_SKILL, BOOSTED_ATTACK_SKILL});
		(*client)->clearSent();
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		if (caster.player) {
			caster.player->setCasting(nullptr); // a cast in progress holds the caster (Skill.effector): the cycle is cut here
			caster.player->setClientConnection(nullptr);
		}
		client.reset();
		caster = {};
		npcs.clear();
		spawnTemplate.reset();
		group.reset();
		mapInstance = nullptr;
		map = nullptr;
		scope.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::MOTION_DATA.resetForTests();
		dataholders::DataManager::SKILL_CHARGE_DATA.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
	}

	/** Places the object in the test map instance, so getPosition()->getWorldMapInstance() answers (Java: World.setPosition) */
	void place(gameserver::model::gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(210010000, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true);
	}

	/** The caster's skill list holds exactly these skills at level 1 (Java PlayerSkillListDAO rows, D3 of m5b2-plan.md) */
	void learn(std::initializer_list<int32_t> skillIds) {
		std::vector<runtime::Ptr<gameserver::model::skill::PlayerSkillEntry>> entries;
		for (int32_t skillId : skillIds) {
			runtime::Ref<gameserver::model::skill::PlayerSkillEntry> entry =
				gameserver::model::skill::PlayerSkillEntry::create(skillId, 1, 0, gameserver::model::gameobjects::Persistable_PersistentState::NOACTION);
			entries.push_back(runtime::Ptr<gameserver::model::skill::PlayerSkillEntry>(entry));
			learned.push_back(std::move(entry));
		}
		caster.player->setSkillList(gameserver::model::skill::PlayerSkillList::create(entries));
	}

	/** A level-1 MONSTER at the given spot, with the effect controller and known list VisibleObjectSpawner gives a spawned npc */
	runtime::Ref<CastTestNpc> spawnMonster(float x, float y, float z) {
		runtime::Ref<CastTestNpc> npc = gameserver::model::gameobjects::VisibleObject::create<CastTestNpc>(std::make_unique<controllers::NpcController>(),
			*spawnTemplate, monsterTemplate());
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		place(*npc, x, y, z);
		npcs.push_back(npc);
		return npc;
	}

	/** spawnMonster with 0 HP from the start: Creature.isDead() is `currentHp == 0` (CreatureLifeStats.java) */
	runtime::Ref<CastTestNpc> spawnCorpse(float x, float y, float z) {
		castTestNpcStartHp = 0;
		runtime::Ref<CastTestNpc> npc = spawnMonster(x, y, z);
		castTestNpcStartHp = 1000;
		return npc;
	}

	const model::SkillTemplate* skillTemplate(int32_t skillId) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId); }

	/** Java `new Skill(template, player, firstTarget)`: the level comes from the skill list */
	runtime::Ref<model::Skill> skill(int32_t skillId, runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget = nullptr) {
		return model::Skill::create(skillTemplate(skillId), *caster.player, firstTarget);
	}

	/** Moves the clock, running the due tasks of the deterministic executor at their due times */
	size_t advance(std::chrono::milliseconds dt) { return executor->advance(dt); }

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	int32_t currentMp() { return caster.player->getLifeStats()->getCurrentMp(); }

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
	std::unique_ptr<runtime::TaskScope> scope;
	xml::LoadContext skillContext;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	runtime::Ref<gameserver::model::templates::spawns::SpawnGroup> group;
	runtime::Ref<CastTestSpawnTemplate> spawnTemplate;
	cp::PlayerFixture caster;
	std::unique_ptr<cp::TestClient> client;
	std::vector<runtime::Ref<gameserver::model::skill::PlayerSkillEntry>> learned;
	std::vector<runtime::Ref<CastTestNpc>> npcs;

	/** Bound on first use, not during static initialization (the binder's own statics may not be initialized yet at that point) */
	static const gameserver::model::templates::npc::NpcTemplate* monsterTemplate() {
		static const gameserver::model::templates::npc::NpcTemplate* bound =
			castTestNpcTemplate(R"(npc_id="210663" level="1" name="monster" rating="NORMAL" rank="VETERAN" tribe="MONSTER" cast_speed="750")",
				R"(<stats maxHp="1000" maxMp="100" attack="10" pdef="10" evasion="20" accuracy="60" pcrit="10">)"
				R"(<speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats>)");
		return bound;
	}
};

} // namespace aion::gameserver::skillengine::test
