// P5-04, M5e stage 3, M-03 (m5e-plan.md §2.5, §5): the summon family of effects - SummonEffect (the Spiritmaster's spirits), SummonServantEffect
// (the Priest's Divine Mirror), SummonTotemEffect (the Gladiator's Battle Banner), SummonTrapEffect, SummonHomingEffect (the Spiritmaster's
// Cyclone Servant), SummonSkillAreaEffect (the Sorcerer's Ice Sheet) and PetOrderUseUltraSkillEffect (the spirit orders).
//
// Each case drives a real Effect through calculate -> applyEffect (EffectsMzTestSupport.h) on skill templates copied verbatim from
// skill_templates.xml (without their conditions and motions), with npcs copied verbatim from npc_templates.xml; the AIs not ported yet
// (servant, homing, skillarea, trap: M-04) are DummyNpcAIs under gameserver.dev.missing_ai_handlers=warn. The spawned objects go
// into the effect lane's Poeta instance; the despawn and skill tasks run on the fixture's DeterministicExecutor. The expectations follow
// SummonEffect.java:24-38, SummonServantEffect.java:24-55, SummonTotemEffect.java:20-49, SummonTrapEffect.java:24-50,
// SummonHomingEffect.java:28-66, SummonSkillAreaEffect.java:22-71 and PetOrderUseUltraSkillEffect.java:24-60.

#include "EffectsMzTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PetSkillData.bind.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/summons/SkillOrder.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_USESKILL.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::Homing;
using gameserver::model::gameobjects::NpcObjectType;
using gameserver::model::gameobjects::Servant;
using gameserver::model::gameobjects::Summon;
using gameserver::model::gameobjects::Trap;
using network::aion::serverpackets::SM_SUMMON_USESKILL;
using services::summons::SummonsService;

// ------------------------------------------------------------------------------------------------------------------------- the data

/**
 * The skills of the cases, verbatim from skill_templates.xml but for their conditions and motions: 3644 "Summon: Earth Spirit", 21 "Summon
 * Divine Mirror", 657 "Battle Banner" (FI_WARFLAG), 289 "Generate Small Aetheric Field" (the trap), 3797 "Summon Cyclone Servant", 1308 "Ice
 * Sheet" (WI_FROSTPILLAR), 3835 "Spirit Detonation Claw" (an order) and 22107 "Command: Earth Detonation Claw" (the spirit's skill it orders).
 * Test templates (64701-64799) where a case needs a value the data does not carry; each names the template it varies.
 */
constexpr const char* SUMMON_EFFECT_SKILLS_XML =
	R"(<skill_template skill_id="3644" name="Summon: Earth Spirit" nameId="2287414" cooldownId="1045" group="EL_SUMMON_EARTHELEMENTAL")"
	R"( stack="EL_LIGHT_SUMMON_EARTHELEMENTAL" lvl="1" skilltype="MAGICAL" skillsubtype="SUMMON" tslot="NONE" activation="ACTIVE" cooldown="50")"
	R"( duration="4500" cancel_rate="30" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="ME" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" /><effects>)"
	R"(<summon npc_id="833287" e="1" noresist="true" element="EARTH" hoptype="SKILLLV" hopb="2295" /></effects></skill_template>)"
	// 64701: 3644 with a live time of 2 s
	R"(<skill_template skill_id="64701" name="mz timed spirit" nameId="1" stack="MZ_TIMED_SPIRIT" lvl="1" skilltype="MAGICAL" skillsubtype="SUMMON")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME" first_target_range="1" target_relation="FRIEND")"
	R"( target_type="ONLYONE" /><effects><summon npc_id="833287" time="2" e="1" noresist="true" element="EARTH" /></effects></skill_template>)"
	R"(<skill_template skill_id="21" name="Summon Divine Mirror" nameId="2285941" cooldownId="1066" group="PR_HOLYSILIKA" stack="PR_LIGHT_HOLYSILIKA")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20")"
	R"( hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true">)"
	R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" /><effects>)"
	R"(<summonservant time="17" npc_id="833061" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="982" /></effects></skill_template>)"
	R"(<skill_template skill_id="657" name="Battle Banner" nameId="2287761" cooldownId="1025" group="FI_WARFLAG" stack="FI_DARK_WARFLAG" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="SUMMON" tslot="NONE" dispel_category="DEBUFF_PHYSICAL" activation="ACTIVE" cooldown="1836")"
	R"( cooldown_delta_lv="-36" duration="0" cancel_rate="30" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true" apply_casting_time_bonus="true"><properties first_target="POINT" first_target_range="20")"
	R"( target_relation="ENEMY" target_type="POINT" target_maxcount="1" /><effects>)"
	R"(<summontotem time="7" npc_id="833078" e="1" noresist="true" element="EARTH" hoptype="SKILLLV" hopb="400" /></effects></skill_template>)"
	// 64702: 657 as a self-targeted Taunting Spirit (PR_PROVOKESERVENT); 64703: 657 targeted, in no special group
	R"(<skill_template skill_id="64702" name="mz provoking totem" nameId="1" group="PR_PROVOKESERVENT" stack="MZ_TOTEM_2" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="SUMMON" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME" first_target_range="1")"
	R"( target_relation="FRIEND" target_type="ONLYONE" /><effects><summontotem time="7" npc_id="833078" e="1" noresist="true" /></effects>)"
	R"(</skill_template>)"
	R"(<skill_template skill_id="64703" name="mz plain totem" nameId="1" group="MZ_TOTEM" stack="MZ_TOTEM_3" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="SUMMON" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="25")"
	R"( target_relation="ENEMY" target_type="ONLYONE" /><effects><summontotem time="7" npc_id="833078" e="1" noresist="true" /></effects>)"
	R"(</skill_template>)"
	R"(<skill_template skill_id="289" name="Generate Small Aetheric Field" nameId="296505" stack="Q_LI_TRAPATKSTUMBLE" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="SUMMONTRAP" tslot="NONE" activation="ACTIVE" cooldown="1200" duration="1000" cancel_rate="20")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><properties first_target="ME" first_target_range="1")"
	R"( target_relation="FRIEND" target_type="ONLYONE" /><effects><summontrap time="60" npc_id="749211" e="1" hoptype="SKILLLV" hopb="2819" />)"
	R"(</effects></skill_template>)"
	// 64704: 289 thrown at a point (not self-targeted): the trap stands where the skill was aimed
	R"(<skill_template skill_id="64704" name="mz thrown trap" nameId="1" stack="MZ_TRAP" lvl="1" skilltype="MAGICAL" skillsubtype="SUMMONTRAP")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="POINT" first_target_range="20")"
	R"( target_relation="ENEMY" target_type="POINT" /><effects><summontrap time="60" npc_id="749211" e="1" /></effects></skill_template>)"
	R"(<skill_template skill_id="3797" name="Summon Cyclone Servant" nameId="2287490" cooldownId="1641" group="EL_SLAVE_STORMSERVENT")"
	R"( stack="EL_LIGHT_SLAVE_STORMSERVENT" lvl="1" skilltype="MAGICAL" skillsubtype="SUMMONHOMING" tslot="NONE" activation="ACTIVE")"
	R"( cooldown="300" duration="1500" stigma="ADVANCED" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true" apply_casting_time_bonus="true"><properties first_target="TARGET" first_target_range="25")"
	R"( target_relation="ENEMY" target_type="ONLYONE" revision_distance="12" /><effects><summonhoming attack_count="1" npc_count="4" time="1")"
	R"( npc_id="833371" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1" /></effects></skill_template>)"
	// 64705: 3797 with three attacks per homing
	R"(<skill_template skill_id="64705" name="mz patient homing" nameId="1" stack="MZ_HOMING" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="SUMMONHOMING" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET")"
	R"( first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" /><effects><summonhoming attack_count="3" npc_count="1" time="1")"
	R"( npc_id="833371" e="1" noresist="true" /></effects></skill_template>)"
	R"(<skill_template skill_id="1308" name="Ice Sheet" nameId="2288028" cooldownId="1457" group="WI_FROSTPILLAR" stack="WI_LIGHT_FROSTPILLAR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="1836" cooldown_delta_lv="-36" duration="0" stigma="BASIC" cancel_rate="30" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true"><properties first_target="POINT")"
	R"( first_target_range="25" target_relation="ENEMY" target_type="POINT" target_maxcount="1" /><effects>)"
	R"(<summonskillarea time="15" npc_id="833209" e="1" hoptype="SKILLLV" hopb="1" /></effects></skill_template>)"
	// 64706-64708: 1308 in the three groups SummonSkillAreaEffect gives another tick or duration
	R"(<skill_template skill_id="64706" name="mz threatening wave" nameId="1" group="KN_THREATENINGWAVE" stack="MZ_AREA_1" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="POINT")"
	R"( first_target_range="25" target_relation="ENEMY" target_type="POINT" /><effects><summonskillarea time="5" npc_id="833209" e="1" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="64707" name="mz tornado" nameId="1" group="WI_SUMMONTORNADO" stack="MZ_AREA_2" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="POINT")"
	R"( first_target_range="25" target_relation="ENEMY" target_type="POINT" /><effects><summonskillarea time="5" npc_id="833209" e="1" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="64708" name="mz delayed strike" nameId="1" group="WI_DELAYEDSTRIKE" stack="MZ_AREA_3" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="POINT")"
	R"( first_target_range="25" target_relation="ENEMY" target_type="POINT" /><effects><summonskillarea time="5" npc_id="833209" e="1" />)"
	R"(</effects></skill_template>)"
	// 64710: the skill area's tick skill, without effects (17500 "Ice Sheet I Effect" would deal damage, which is not what these cases look at)
	R"(<skill_template skill_id="64710" name="mz area tick" nameId="1" stack="MZ_AREA_TICK" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME" first_target_range="1" target_relation="FRIEND")"
	R"( target_type="ONLYONE" /></skill_template>)"
	R"(<skill_template skill_id="3835" name="Spirit Detonation Claw" nameId="2287580" cooldownId="1659" group="EL_ORDER_ASSAIL" stack="EL_ORDER_ASSAIL")"
	R"( lvl="1" skilltype="MAGICAL" skill_category="CHAIN_SKILL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="180")"
	R"( duration="800" cancel_rate="5" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")"
	R"( apply_casting_time_bonus="true"><properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE")"
	R"( revision_distance="12" /><effects><petorderuseultraskill ultra_skill="5" e="1" noresist="true" hoptype="SKILLLV" hopb="1884" />)"
	R"(</effects></skill_template>)"
	// 64709: 3835 with release="true" (the orders that send the spirit away after the skill)
	R"(<skill_template skill_id="64709" name="mz releasing order" nameId="1" stack="MZ_ORDER" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="25")"
	R"( target_relation="ENEMY" target_type="ONLYONE" /><effects><petorderuseultraskill release="true" e="1" noresist="true" /></effects>)"
	R"(</skill_template>)"
	R"(<skill_template skill_id="22107" name="Command: Earth Detonation Claw" nameId="2287585" stack="EL_N_ORDER_ASSAIL_EARTH" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><properties first_target="TARGET" first_target_range="5")"
	R"( target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" /><effects><spellatkinstant value="439" e="1" element="EARTH")"
	R"( hoptype="DAMAGE" /></effects></skill_template>)";

/** npc_templates.xml: the summoned npcs of the skills above, verbatim */
constexpr std::string_view SUMMON_EFFECT_NPCS_XML =
	R"(<npc_templates>)"
	R"(<npc_template npc_id="833287" level="16" name="earth spirit" name_id="466282" height="1" group_drop="ELEMENTALEARTH1" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="ELEMENTAL" tribe="PET" type="SUMMON_PET" srange="15" arange="2" attack_speed="2040" hpgauge="3" cancel_level="90">)"
	R"(<stats maxHp="1575" attack="85" pdef="575" mresist="340" accuracy="503" macc="291" pcrit="50" mcrit="18" evasion="503" parry="0">)"
	R"(<speeds walk="2" group_walk="1.2" run="8.4" run_fight="8.4" group_run_fight="8" /></stats>)"
	R"(<bound_radius front="0.25" side="0.25" upper="2.8" /></npc_template>)"
	R"(<npc_template npc_id="833061" level="1" name="divine mirror" name_id="466260" height="1.02" group_drop="SILIKA" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="MAGICALMONSTER" tribe="GENERAL" type="ABYSS_GUARD" ai="servant" srange="10" attack_speed="3000" hpgauge="3">)"
	R"(<stats maxHp="102" /><bound_radius front="0.45" side="0.56" upper="2.82" /></npc_template>)"
	R"(<npc_template npc_id="833078" level="65" name="war flag" name_id="466262" height="1.224" group_drop="NONE" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="ABYSS_GUARD" ai="servant" srange="4" attack_speed="3000" hpgauge="3">)"
	R"(<stats maxHp="6114" /><bound_radius front="0.54" side="0.672" upper="1.224" /></npc_template>)"
	R"(<npc_template npc_id="749211" level="54" name="small aether generator" name_id="392067" height="3" group_drop="NONE" rank="NOVICE")"
	R"( rating="JUNK" race="ELYOS" tribe="ATKDRAKAN" type="ABYSS_GUARD" ai="trap" srange="4" attack_speed="2000" hpgauge="1"><stats maxHp="1" />)"
	R"(<bound_radius front="0.6" side="1.728" upper="3" /></npc_template>)"
	R"(<npc_template npc_id="833371" level="45" name="energy of cyclone" name_id="466288" height="1.3259999" group_drop="NONE" rank="NOVICE")"
	R"( rating="JUNK" race="MAGICALMONSTER" tribe="GENERAL" type="ABYSS_GUARD" ai="homing" srange="10" arange="2" attack_speed="3000" hpgauge="1">)"
	R"(<stats maxHp="3604"><speeds walk="2" group_walk="2" run="30" run_fight="30" group_run_fight="30" /></stats></npc_template>)"
	R"(<npc_template npc_id="833209" level="20" name="ice sheet" name_id="466273" height="3" group_drop="NONE" rank="NOVICE" rating="JUNK")"
	R"( race="ELYOS" tribe="GENERAL" type="ABYSS_GUARD" ai="skillarea" srange="15" attack_speed="2000" hpgauge="1"><stats maxHp="1860" />)"
	R"(<bound_radius front="0.6" side="1.728" upper="3" /></npc_template>)"
	R"(</npc_templates>)";

/** npc_skills.xml's row of the ice sheet, with the test tick skill 64710 for its 17500 */
constexpr std::string_view SUMMON_EFFECT_NPC_SKILLS_XML =
	R"(<npc_skill_templates><npc_skills npc_ids="833209"><npc_skill id="64710" lv="1" prob="100" /></npc_skills></npc_skill_templates>)";

/** pet_skills.xml's row of 3835 for the earth spirit 833287, and 64709 for it too; 64711 orders a skill no template carries */
constexpr std::string_view SUMMON_EFFECT_PET_SKILLS_XML =
	R"(<pet_skill_templates><pet_skill skill_id="22107" pet_id="833287" order_skill="3835"/>)"
	R"(<pet_skill skill_id="22107" pet_id="833287" order_skill="64709"/></pet_skill_templates>)";

constexpr int32_t EARTH_SPIRIT = 833287;
constexpr int32_t DIVINE_MIRROR = 833061;
constexpr int32_t WAR_FLAG = 833078;
constexpr int32_t AETHER_GENERATOR = 749211;
constexpr int32_t CYCLONE = 833371;
constexpr int32_t ICE_SHEET = 833209;

class SummonEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
	// C++ only (gameserver.dev.missing_ai_handlers=warn, docs/deviations/P4-01.md): the npcs keep their ai names; the ones not ported yet
	// (servant, homing, skillarea, trap: M-04) get the NpcAI-derived DummyNpcAI from AIEngine.newAI
		missingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + SUMMON_EFFECT_SKILLS_XML);
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcDataContext, std::string(SUMMON_EFFECT_NPCS_XML)));
		npcDataPublished = true;
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(
			xml::bindString<dataholders::NpcSkillData>(dataContext, std::string(SUMMON_EFFECT_NPC_SKILLS_XML)));
		dataholders::DataManager::PET_SKILL_DATA.publish(
			xml::bindString<dataholders::PetSkillData>(dataContext, std::string(SUMMON_EFFECT_PET_SKILLS_XML)));
		caster = player(9301, PlayerClass::SPIRIT_MASTER, 16);
		addToRegion(*caster);
		npc = monster(510, 500, 100);
		pair(*npc, *caster);
	}

	void TearDown() override {
		{
			EFFECT_TEST_SCOPE;
			for (const Ptr<Npc>& left : spawnedNpcs())
				left->getController().delete_();
			if (Ptr<Summon> summon = caster->getSummon())
				summon->getController().delete_();
			caster->setSummon(nullptr);
		}
		caster = nullptr;
		npc = nullptr;
		dataholders::DataManager::PET_SKILL_DATA.resetForTests();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(*missingAiHandlers);
		EffectsMzTest::TearDown();
	}

	/** The spawned npcs of the template in the World (SpawnEngine.bringIntoWorld stores them there), in object id order */
	std::vector<Ptr<Npc>> spawned(int32_t npcId) {
		std::vector<Ptr<Npc>> npcs;
		world::World::getInstance().forEachObject([&npcs, npcId](gameserver::model::gameobjects::VisibleObject& object) {
			if (Ptr<Npc> found = runtime::as<Npc>(object); found && found->getNpcId() == npcId)
				npcs.push_back(found);
		});
		std::sort(npcs.begin(), npcs.end(), [](const Ptr<Npc>& a, const Ptr<Npc>& b) { return a->getObjectId() < b->getObjectId(); });
		return npcs;
	}

	std::vector<Ptr<Npc>> spawnedNpcs() {
		std::vector<Ptr<Npc>> npcs;
		for (int32_t npcId : {DIVINE_MIRROR, WAR_FLAG, AETHER_GENERATOR, CYCLONE, ICE_SHEET})
			for (const Ptr<Npc>& found : spawned(npcId))
				npcs.push_back(found);
		return npcs;
	}

	/**
	 * A cast of the skill by the caster at the target, aimed at x, y, z (Skill.create, Skill.setTargetPosition), its Effect calculated, placed
	 * and applied in Skill.applyEffect's order (Skill.java:587-595: new Effect, initialize, setWorldPosition)
	 */
	Ref<Effect> cast(int32_t skillId, Creature& target, float x = 0, float y = 0, float z = 0) {
		Ref<model::Skill> skill = model::Skill::create(skillTemplate(skillId), *caster, Ptr<Creature>(target), 1);
		skill->setTargetPosition(x, y, z, 0);
		skills.push_back(skill);
		Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(target));
		effect->initialize();
		effect->setWorldPosition(caster->getWorldId(), caster->getInstanceId(), x, y, z);
		effect->applyEffect();
		return effect;
	}

	/** The SM_CASTSPELL_RESULTs of the servant's casts the caster was sent (the caster and the servant are paired by pairWith) */
	int64_t castsOf(Npc& servant) {
		int64_t casts = 0;
		for (const std::vector<uint8_t>& bytes : sentTo<network::aion::serverpackets::SM_CASTSPELL_RESULT>(*caster)) {
			network::test::PacketReader reader(cp::bodyOf(bytes));
			if (reader.D() == servant.getObjectId())
				++casts;
		}
		return casts;
	}

	void pairWith(Npc& servant) { pair(servant, *caster); }

	Ref<Player> caster;
	Ref<Npc> npc;
	std::vector<Ref<model::Skill>> skills;
	xml::LoadContext dataContext;
	std::shared_ptr<const std::string> missingAiHandlers;
};

// ------------------------------------------------------------------------------------------------------------------------------ SummonEffect

TEST_F(SummonEffectsTest, TheSummonSkillCallsTheSpiritForTheEffected) {
	EFFECT_TEST_SCOPE;
	Ref<Effect> effect = applied(3644, *caster, *caster);

	ASSERT_TRUE(effect->isInSuccessEffects(1)) << "SummonEffect.calculate adds it without a resist roll";
	Ptr<Summon> spirit = caster->getSummon();
	ASSERT_TRUE(spirit);
	EXPECT_EQ(spirit->getNpcId(), EARTH_SPIRIT);
	EXPECT_EQ(spirit->getSummonedBySkillId(), 3644);
	EXPECT_FALSE(spirit->getController().hasTask(gameserver::model::TaskId::DESPAWN)) << "3644 has no live time";
	advance(60000);
	EXPECT_TRUE(spirit->isSpawned());
}

TEST_F(SummonEffectsTest, ASpiritWithALiveTimeIsReleasedWhenItRunsOut) {
	EFFECT_TEST_SCOPE;
	applied(64701, *caster, *caster);
	Ref<Summon> spirit(caster->getSummon());
	ASSERT_TRUE(spirit);
	EXPECT_TRUE(spirit->getController().hasTask(gameserver::model::TaskId::DESPAWN));

	advance(1999);
	EXPECT_TRUE(spirit->isSpawned());
	advance(1);
	EXPECT_FALSE(spirit->isSpawned()) << "UNSPECIFIED is an instant release";
	EXPECT_FALSE(caster->getSummon());
}

TEST_F(SummonEffectsTest, ASecondSpiritIsRefusedAndGetsNoLiveTimeTask) {
	EFFECT_TEST_SCOPE;
	applied(3644, *caster, *caster);
	Ref<Summon> first(caster->getSummon());

	applied(64701, *caster, *caster);

	EXPECT_EQ(caster->getSummon(), Ptr<Summon>(first));
	EXPECT_FALSE(first->getController().hasTask(gameserver::model::TaskId::DESPAWN)) << "the refused summon schedules nothing";
	advance(2000);
	EXPECT_TRUE(first->isSpawned());
}

// ----------------------------------------------------------------------------------------------------------------------- SummonServantEffect

TEST_F(SummonEffectsTest, TheDivineMirrorStandsTwoMetresAheadOfTheCasterUntilItsTimeIsUp) {
	EFFECT_TEST_SCOPE;
	cast(21, *npc);

	std::vector<Ptr<Npc>> mirrors = spawned(DIVINE_MIRROR);
	ASSERT_EQ(mirrors.size(), 1u);
	Servant& mirror = *runtime::cast<Servant>(mirrors[0]);
	EXPECT_EQ(mirror.getNpcObjectType(), NpcObjectType::SERVANT);
	EXPECT_EQ(mirror.getCreator(), Ptr<gameserver::model::gameobjects::VisibleObject>(*caster));
	// heading 0: two metres along x (GeoService answers the point itself without geo data)
	EXPECT_FLOAT_EQ(mirror.getX(), caster->getX() + 2);
	EXPECT_FLOAT_EQ(mirror.getY(), caster->getY());
	EXPECT_TRUE(mirror.getController().hasTask(gameserver::model::TaskId::DESPAWN));

	advance(17 * 1000 + 2999);
	EXPECT_TRUE(mirror.isSpawned());
	advance(1);
	EXPECT_FALSE(mirror.isSpawned()) << "the live time plus INITIAL_SPAWN_DELAY";
}

TEST_F(SummonEffectsTest, AServantWithoutATargetIsRefusedUnlessTheSkillIsAimedAtAPoint) {
	EFFECT_TEST_SCOPE;
	Ref<model::Skill> skill = model::Skill::create(skillTemplate(21), *caster, Ptr<Creature>(npc), 1);
	skills.push_back(skill);
	Ref<Effect> effect = Effect::create(*skill, nullptr);

	// the template's own applyEffect: Effect.applyEffect would wrap the exception ("Error applying effect of skill 21 ...")
	EXPECT_THROW(skillTemplate(21)->getEffects()->getEffects()[0]->applyEffect(*effect), runtime::IllegalArgumentException);
	EXPECT_TRUE(spawned(DIVINE_MIRROR).empty());
}

// ------------------------------------------------------------------------------------------------------------------------ SummonTotemEffect

TEST_F(SummonEffectsTest, TheBattleBannerStandsWhereItWasAimedForFifteenSeconds) {
	EFFECT_TEST_SCOPE;
	cast(657, *npc, 520, 510, 100);

	std::vector<Ptr<Npc>> flags = spawned(WAR_FLAG);
	ASSERT_EQ(flags.size(), 1u);
	Servant& flag = *runtime::cast<Servant>(flags[0]);
	EXPECT_EQ(flag.getNpcObjectType(), NpcObjectType::TOTEM);
	EXPECT_FLOAT_EQ(flag.getX(), 520);
	EXPECT_FLOAT_EQ(flag.getY(), 510);

	advance(15 * 1000 + 2999); // FI_WARFLAG: 15 s instead of the template's 7
	EXPECT_TRUE(flag.isSpawned());
	advance(1);
	EXPECT_FALSE(flag.isSpawned());
}

TEST_F(SummonEffectsTest, ABannerAimedAtNoPointStandsAtTheCaster) {
	EFFECT_TEST_SCOPE;
	cast(657, *npc); // the skill's target position stays 0, 0

	std::vector<Ptr<Npc>> flags = spawned(WAR_FLAG);
	ASSERT_EQ(flags.size(), 1u);
	EXPECT_FLOAT_EQ(flags[0]->getX(), caster->getX());
	EXPECT_FLOAT_EQ(flags[0]->getY(), caster->getY());
}

TEST_F(SummonEffectsTest, ATauntingSpiritStandsBeforeTheEffectedForTwentySeconds) {
	EFFECT_TEST_SCOPE;
	cast(64702, *caster);

	std::vector<Ptr<Npc>> totems = spawned(WAR_FLAG);
	ASSERT_EQ(totems.size(), 1u);
	EXPECT_FLOAT_EQ(totems[0]->getX(), caster->getX() + 2) << "self-targeted: two metres before the effected along the caster's heading";
	advance(20 * 1000 + 2999); // PR_PROVOKESERVENT: 20 s
	EXPECT_TRUE(totems[0]->isSpawned());
	advance(1);
	EXPECT_FALSE(totems[0]->isSpawned());
}

TEST_F(SummonEffectsTest, ATotemOfNoSpecialGroupStandsAtTheCasterForItsOwnTime) {
	EFFECT_TEST_SCOPE;
	cast(64703, *npc, 520, 510, 100); // neither self-targeted nor aimed at a point: the target position is not read

	std::vector<Ptr<Npc>> totems = spawned(WAR_FLAG);
	ASSERT_EQ(totems.size(), 1u);
	EXPECT_FLOAT_EQ(totems[0]->getX(), caster->getX());
	EXPECT_FLOAT_EQ(totems[0]->getY(), caster->getY());
	advance(7 * 1000 + 2999);
	EXPECT_TRUE(totems[0]->isSpawned());
	advance(1);
	EXPECT_FALSE(totems[0]->isSpawned());
}

// ------------------------------------------------------------------------------------------------------------------------- SummonTrapEffect

TEST_F(SummonEffectsTest, ASelfTargetedTrapIsLaidBeforeTheCasterAndRemovedAfterAMinute) {
	EFFECT_TEST_SCOPE;
	ASSERT_FALSE(caster->getTarget());
	cast(289, *caster);

	EXPECT_EQ(caster->getTarget(), Ptr<gameserver::model::gameobjects::VisibleObject>(*caster)) << "no target: the caster targets itself";
	std::vector<Ptr<Npc>> traps = spawned(AETHER_GENERATOR);
	ASSERT_EQ(traps.size(), 1u);
	ASSERT_TRUE(runtime::as<Trap>(traps[0]));
	EXPECT_FLOAT_EQ(traps[0]->getX(), caster->getX() + 2);
	EXPECT_EQ(traps[0]->getCreatorId(), caster->getObjectId());

	advance(59999);
	EXPECT_TRUE(traps[0]->isSpawned());
	advance(1);
	EXPECT_FALSE(traps[0]->isSpawned());
}

TEST_F(SummonEffectsTest, AThrownTrapLandsWhereItWasAimedAndATargetIsKept) {
	EFFECT_TEST_SCOPE;
	caster->setTarget(npc);
	cast(64704, *npc, 515, 505, 100);

	EXPECT_EQ(caster->getTarget(), Ptr<gameserver::model::gameobjects::VisibleObject>(*npc));
	std::vector<Ptr<Npc>> traps = spawned(AETHER_GENERATOR);
	ASSERT_EQ(traps.size(), 1u);
	EXPECT_FLOAT_EQ(traps[0]->getX(), 515);
	EXPECT_FLOAT_EQ(traps[0]->getY(), 505);
}

TEST_F(SummonEffectsTest, AThirdTrapOfTheCasterRemovesItsFirst) {
	EFFECT_TEST_SCOPE;
	cast(64704, *npc, 515, 505, 100);
	Ref<Npc> first(spawned(AETHER_GENERATOR)[0]);
	cast(64704, *npc, 516, 505, 100);
	cast(64704, *npc, 517, 505, 100);

	EXPECT_FALSE(first->isSpawned()) << "TrapService.registerTrap(..., true)";
	EXPECT_EQ(spawned(AETHER_GENERATOR).size(), 2u);
}

// ------------------------------------------------------------------------------------------------------------------------ SummonHomingEffect

TEST_F(SummonEffectsTest, TheCycloneServantSendsFourHomingsThatEachAttackOnce) {
	EFFECT_TEST_SCOPE;
	cast(3797, *npc);

	std::vector<Ptr<Npc>> homings = spawned(CYCLONE);
	ASSERT_EQ(homings.size(), 4u);
	for (const Ptr<Npc>& spawnedHoming : homings) {
		Homing& homing = *runtime::cast<Homing>(spawnedHoming);
		EXPECT_EQ(homing.getAttackCount(), 1);
		EXPECT_FLOAT_EQ(homing.getX(), caster->getX());
		EXPECT_TRUE(homing.getController().hasTask(gameserver::model::TaskId::DESPAWN));
	}

	Homing& first = *runtime::cast<Homing>(homings[0]);
	first.getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_EQ(first.getAttackCount(), 0);
	EXPECT_FALSE(first.isSpawned()) << "its last attack deletes it";
	EXPECT_TRUE(homings[1]->isSpawned());
}

TEST_F(SummonEffectsTest, AHomingCountsItsAttacksDownAndIsRemovedAfterFifteenSecondsAnyway) {
	EFFECT_TEST_SCOPE;
	cast(64705, *npc);

	std::vector<Ptr<Npc>> homings = spawned(CYCLONE);
	ASSERT_EQ(homings.size(), 1u);
	Homing& homing = *runtime::cast<Homing>(homings[0]);
	homing.getObserveController()->notifyAttackObservers(*npc, 0);
	homing.getObserveController()->notifyAttackObservers(*npc, 0);
	EXPECT_EQ(homing.getAttackCount(), 1);
	EXPECT_TRUE(homing.isSpawned());

	advance(14999);
	EXPECT_TRUE(homing.isSpawned());
	advance(1);
	EXPECT_FALSE(homing.isSpawned());
}

// --------------------------------------------------------------------------------------------------------------------- SummonSkillAreaEffect

TEST_F(SummonEffectsTest, TheIceSheetStandsWhereItWasAimedAndCastsEveryThreeSeconds) {
	EFFECT_TEST_SCOPE;
	advance(100);
	cast(1308, *npc, 512, 506, 100);

	std::vector<Ptr<Npc>> sheets = spawned(ICE_SHEET);
	ASSERT_EQ(sheets.size(), 1u);
	Servant& sheet = *runtime::cast<Servant>(sheets[0]);
	EXPECT_EQ(sheet.getNpcObjectType(), NpcObjectType::SKILLAREA);
	EXPECT_FLOAT_EQ(sheet.getX(), 512);
	EXPECT_FLOAT_EQ(sheet.getY(), 506);
	EXPECT_TRUE(sheet.getController().hasTask(gameserver::model::TaskId::SKILL_USE));
	pairWith(sheet);

	advance(0);
	EXPECT_EQ(castsOf(sheet), 1) << "the first tick runs at once";
	advance(2999);
	EXPECT_EQ(castsOf(sheet), 1);
	advance(1);
	EXPECT_EQ(castsOf(sheet), 2);

	advance(15 * 1000 + 3000 - 3000 - 1); // the live time plus INITIAL_SPAWN_DELAY, from the cast
	EXPECT_TRUE(sheet.isSpawned());
	advance(1);
	EXPECT_FALSE(sheet.isSpawned());
}

TEST_F(SummonEffectsTest, AnAreaAimedAtNoPointStandsOnTheEffected) {
	EFFECT_TEST_SCOPE;
	cast(1308, *npc);

	std::vector<Ptr<Npc>> sheets = spawned(ICE_SHEET);
	ASSERT_EQ(sheets.size(), 1u);
	EXPECT_FLOAT_EQ(sheets[0]->getX(), npc->getX());
	EXPECT_FLOAT_EQ(sheets[0]->getY(), npc->getY());
}

TEST_F(SummonEffectsTest, ThreeGroupsChangeTheTickOrTheLiveTime) {
	EFFECT_TEST_SCOPE;
	struct Expected {
		int32_t skillId;
		int64_t tick;
		int64_t liveTime; // seconds, before INITIAL_SPAWN_DELAY
	};
	for (const Expected& expected : {Expected{64706, 2000, 15}, Expected{64707, 1900, 5}, Expected{64708, 5000, 9}}) {
		SCOPED_TRACE(expected.skillId);
		advance(100);
		cast(expected.skillId, *npc, 512, 506, 100);
		std::vector<Ptr<Npc>> sheets = spawned(ICE_SHEET);
		ASSERT_EQ(sheets.size(), 1u);
		Npc& sheet = *sheets[0];
		pairWith(sheet);
		clearSent(*caster);
		advance(0);
		EXPECT_EQ(castsOf(sheet), 1);
		advance(expected.tick - 1);
		EXPECT_EQ(castsOf(sheet), 1);
		advance(1);
		EXPECT_EQ(castsOf(sheet), 2);
		advance(expected.liveTime * 1000 + 3000 - expected.tick - 1);
		EXPECT_TRUE(sheet.isSpawned());
		advance(1);
		EXPECT_FALSE(sheet.isSpawned());
	}
}

// ---------------------------------------------------------------------------------------------------------------- PetOrderUseUltraSkillEffect

TEST_F(SummonEffectsTest, AnOrderQueuesTheSpiritsSkillAndTellsTheMaster) {
	EFFECT_TEST_SCOPE;
	Ptr<Summon> spirit = SummonsService::createSummon(*caster, EARTH_SPIRIT, 3644, 1, 0);
	ASSERT_TRUE(spirit);
	clearSent(*caster);

	Ref<Effect> effect = applied(3835, *caster, *npc);

	ASSERT_TRUE(effect->isInSuccessEffects(1));
	Ptr<gameserver::model::summons::SkillOrder> order = spirit->getNextSkillOrder();
	ASSERT_TRUE(order);
	EXPECT_EQ(order->getSkillId(), 22107);
	EXPECT_EQ(order->getSkillLevel(), 1);
	EXPECT_EQ(order->getTarget(), Ptr<Creature>(npc));
	ASSERT_GT(effect->getEffectHate(), 1);
	EXPECT_EQ(order->getHate(), effect->getEffectHate());
	EXPECT_FALSE(order->isRelease());
	EXPECT_EQ(sentTo<SM_SUMMON_USESKILL>(*caster),
		(std::vector<std::vector<uint8_t>>{cp::serialized(SM_SUMMON_USESKILL(spirit->getObjectId(), 22107, 1, npc->getObjectId()))}));
}

TEST_F(SummonEffectsTest, AReleasingOrderSaysSo) {
	EFFECT_TEST_SCOPE;
	Ptr<Summon> spirit = SummonsService::createSummon(*caster, EARTH_SPIRIT, 3644, 1, 0);
	Ref<Effect> effect = applied(64709, *caster, *npc);

	ASSERT_TRUE(spirit->getNextSkillOrder());
	EXPECT_TRUE(spirit->getNextSkillOrder()->isRelease());
	ASSERT_LE(effect->getEffectHate(), 1) << "64709 has no hop";
	EXPECT_EQ(spirit->getNextSkillOrder()->getHate(), 0) << "an effect hate of 0 or 1 orders no hate";
}

TEST_F(SummonEffectsTest, NoOrderReachesAMissingOrLeavingSpirit) {
	EFFECT_TEST_SCOPE;
	applied(3835, *caster, *npc); // no spirit: nothing happens
	EXPECT_TRUE(sentTo<SM_SUMMON_USESKILL>(*caster).empty());

	Ptr<Summon> spirit = SummonsService::createSummon(*caster, EARTH_SPIRIT, 3644, 1, 0);
	SummonsService::release(*spirit, gameserver::model::summons::UnsummonType::COMMAND);
	applied(3835, *caster, *npc);

	EXPECT_FALSE(spirit->getNextSkillOrder()) << "a spirit being released takes no order";
	EXPECT_TRUE(sentTo<SM_SUMMON_USESKILL>(*caster).empty());
}

TEST_F(SummonEffectsTest, AnOrderOfAnNpcIsNotCalculated) {
	EFFECT_TEST_SCOPE;
	Ref<Effect> effect = calculated(3835, *npc, *caster);
	EXPECT_FALSE(effect->isInSuccessEffects(1)) << "PetOrderUseUltraSkillEffect.calculate: a Player effector only";

	Ref<Effect> untargeted = Effect::create(*caster, nullptr, skillTemplate(3835), 1);
	untargeted->initialize();
	EXPECT_FALSE(untargeted->isInSuccessEffects(1)) << "and an effected";
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
