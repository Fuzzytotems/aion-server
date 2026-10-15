// P5-01 (M5b-2 stage 1 part 3, work item F-06, decision D12 of m5b2-plan.md): the five attack bodies the effect engine calls - the damage a
// skill deals once EffectTemplate.calculate has decided that it lands.
//
// - StatFunctions.calculateMagicalSkillDamage (StatFunctions.java:381-415): the magic boost against the target's boost resist and its 2900 cap,
//   the knowledge factor, BOOST_SPELL_ATTACK, the bonus, the elemental defense and the MDef tenth (skipped for NONE and NoReduceSpellATK), the
//   floor at 0 and the +-8 % roll of an npc that is no SummonedObject.
// - AttackUtil.calculateMagicalOverTimeSkillResult (AttackUtil.java:427-455): the Trap arm, the magical damage multiplier of the effector's
//   observers, the magical critical of the effect position with its critAddDmg and the target's fortitude, the PvP and PvE modifiers, the floor
//   at 1 and the npc AI.
// - AttackUtil.calculatePhysicalStatus(attacker, attacked, template, effect) (AttackUtil.java:457-461): accMod2 + accMod1 * level, cannotmiss
//   (which still consumes the always-dodge/block/parry activations) and critProbMod2 + critProbMod1 * level.
// - AttackUtil.calculateSkillResult (AttackUtil.java:222-351): both arms - physical (weapon damage, block, parry, the weapon group's critical
//   multiplier, the PDef tenth) and magical (the magical critical, calculateMagicalSkillDamage) - the ADD and PERCENT modes, the action modifier,
//   the one-time boost multipliers, the attacker's movement, rnddmg, the SummonedObject arm, the "dirty fix" for physical weapons casting a
//   magical skill, the shared divisor, the PvP and PvE modifiers, the npc AI hooks, and the three template instanceof tests (SkillAttackInstant,
//   NoReduceSpellATKInstant, DelayedSpellAttackInstant / ProcAtkInstant).
// - AttackUtil.calculateEffectResult (AttackUtil.java:387-404): the shield arm (every AttackResult field the observers write is copied into the
//   Effect) and ignoreShield, and the EffectReserved it records.
//
// **Golden vectors.** Every expected number is derived from the Java arithmetic in the comment beside it, one float operation at a time (Java
// float arithmetic is IEEE single precision with round-to-nearest; `f(x)` below means "x rounded to float"). The inputs are the fixture's own:
// the stats a level-1 character has without its enter-world stat functions (the fixture never registers PlayerStatFunctions, so a Mage's
// knowledge is its class value 115, its MDef, PDef, boost and resist stats 0, its parry/block 74 and its accuracy 198), the npc templates below
// (NpcData fills every magical stat the XML leaves at 0, so each one is written out), and stat functions the cases add. Only the rows whose
// derivation passes a random draw replay the seed's reference stream (Rnd is xoshiro256++, not java.util.Random, CONVENTIONS.md).
// Most vectors are exact in float, which keeps the derivations short but cannot see the order of two float operations (m5b2-plan.md risk 11);
// the rows that pin an order (the knowledge factor, the MDef and PDef tenths, movement before the one-time multiplier, rnddmg before the
// critical, the npc AI and the shared divisor before the PvE modifiers, the dot's multiplier before its critical) are inexact on purpose,
// found with a scratch model of Java float arithmetic so that the reordered expression - whose value each comment gives in parentheses -
// lands on the other side of an integer (or of a float step, where the body answers a float).
//
// Every case runs in the Debug build; m5b2-plan.md risk 11 asks for RelWithDebInfo as well - see docs/deviations/P5-01.md, F-06.

#include <gtest/gtest.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/change/Func.h"
#include "aion/gameserver/skillengine/effect/DamageEffect.h"
#include "aion/gameserver/skillengine/effect/DelayedSpellAttackInstantEffect.h"
#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.h"
#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.h"
#include "aion/gameserver/skillengine/effect/SkillAttackInstantEffect.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HitType.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "StatsTestSupport.h"

namespace aion::gameserver::model::stats::test {
namespace {

using calc::functions::IStatFunction;
using calc::functions::RcStatFunction;
using calc::functions::StatAddFunction;
using calc::functions::StatRateFunction;
using container::StatEnum;
using controllers::attack::AttackResult;
using controllers::attack::AttackStatus;
using controllers::attack::AttackUtil;
using runtime::Ptr;
using runtime::Ref;
using skillengine::model::Effect;
using skillengine::model::HitType;
using skillengine::model::SkillTemplate;
using utils::stats::StatFunctions;
namespace Rnd = commons::utils::Rnd;

// ---- access ---------------------------------------------------------------------------------------------------------------------------------

/**
 * AttackUtil.calculatePhysicalStatus(attacker, attacked, template, effect) and calculateEffectResult are private (Java: private static), and so
 * is the modifier list of ActionModifiers. The standard's explicit-instantiation rule reaches them without a test friend in the frozen headers
 * (the names in an explicit instantiation are not access-checked, [temp.spec.general]/6) - the pattern of tests/effects_al/EffectTemplateTest.
 */
template <class Tag, typename Tag::Type Member>
struct PrivateAccess {
	friend typename Tag::Type privateMember(Tag) { return Member; }
};

struct PhysicalStatusOfTemplateTag {
	using Type = AttackStatus (*)(gameobjects::Creature&, gameobjects::Creature&, const skillengine::effect::EffectTemplate*, Effect&);
	friend Type privateMember(PhysicalStatusOfTemplateTag);
};
template struct PrivateAccess<PhysicalStatusOfTemplateTag, &AttackUtil::calculatePhysicalStatus>;

struct EffectResultTag {
	using Type = void (*)(Effect&, gameobjects::Creature&, int32_t, AttackStatus, HitType, bool, int32_t, bool);
	friend Type privateMember(EffectResultTag);
};
template struct PrivateAccess<EffectResultTag, &AttackUtil::calculateEffectResult>;

struct ModifierListTag {
	using Type = std::vector<std::unique_ptr<skillengine::effect::modifier::ActionModifier>> skillengine::effect::modifier::ActionModifiers::*;
	friend Type privateMember(ModifierListTag);
};
template struct PrivateAccess<ModifierListTag, &skillengine::effect::modifier::ActionModifiers::actionModifiers>;

// ---- test doubles ---------------------------------------------------------------------------------------------------------------------------

/** Java DamageEffect is abstract; the plain subclass a case needs when the dynamic type must be none of the special ones */
class PlainDamageEffect : public skillengine::effect::DamageEffect {
public:
	std::string_view javaClassName() const override { return "PlainDamageEffect"; }
};

/** A damage effect template with the JAXB fields the cases set opened; `Base` keeps the dynamic type calculateSkillResult's instanceof tests see */
template <class Base>
class DamageProbe : public Base {
public:
	DamageProbe(SkillElement elementValue, int32_t skillPosition) {
		this->element = elementValue;
		this->position = skillPosition;
	}

	using Base::accMod1;
	using Base::accMod2;
	using Base::critAddDmg1;
	using Base::critAddDmg2;
	using Base::critProbMod1;
	using Base::critProbMod2;
	using Base::mode;
	using Base::modifiers;
	using Base::shared;
};

/** SkillAttackInstantEffect (`skillatk`), the only template with rnddmg and cannotmiss */
class SkillAttackProbe final : public DamageProbe<skillengine::effect::SkillAttackInstantEffect> {
public:
	using DamageProbe::DamageProbe;
	using skillengine::effect::SkillAttackInstantEffect::cannotmiss;
	using skillengine::effect::SkillAttackInstantEffect::rnddmg;
};

using SpellProbe = DamageProbe<PlainDamageEffect>;
using NoReduceProbe = DamageProbe<skillengine::effect::NoReduceSpellATKInstantEffect>;
using DelayedProbe = DamageProbe<skillengine::effect::DelayedSpellAttackInstantEffect>;
using ProcAtkProbe = DamageProbe<skillengine::effect::ProcAtkInstantEffect>;

/** An action modifier whose check always passes (Java: e.g. BackDamageModifier from behind), answering a fixed value in the given mode */
class ProbeModifier final : public skillengine::effect::modifier::ActionModifier {
public:
	ProbeModifier(int32_t answerValue, skillengine::change::Func func) : answer(answerValue) { mode = func; }

	std::string_view javaClassName() const override { return "ProbeModifier"; }

	int32_t analyze(Effect& /*effect*/) const override { return answer; }

	bool check(Effect& /*effect*/) const override { return true; }

private:
	const int32_t answer;
};

std::unique_ptr<skillengine::effect::modifier::ActionModifiers> modifiersOf(int32_t answer, skillengine::change::Func func) {
	auto modifiers = std::make_unique<skillengine::effect::modifier::ActionModifiers>();
	((*modifiers).*privateMember(ModifierListTag{})).push_back(std::make_unique<ProbeModifier>(answer, func));
	return modifiers;
}

/**
 * An AttackCalcObserver standing in for the effects that attach one (AlwaysBlock/Parry/DodgeEffect, OneTimeBoostSkillAttack/CriticalEffect,
 * ShieldEffect, ...). It records what AttackUtil asks and hands it, and answers what the case configures:
 * - checkStatus: true for `alwaysStatus` while `activations` > 0, counting them down like AttackStatusObserver's activation count;
 * - checkShield: records the result it is handed, and with `shield` set writes every field calculateEffectResult copies into the Effect;
 * - checkAttackerCriticalStatus: with `alwaysCritical`, the non-percent status of value 1000 (`Rnd.nextInt(1000) < 1000`, StatFunctions.java:597);
 * - the two base damage multipliers.
 */
class ProbeCalcObserver final : public controllers::observer::AttackCalcObserver {
	AION_MAKE_REF_FRIEND
public:
	static Ref<ProbeCalcObserver> create() { return runtime::makeRef<ProbeCalcObserver>(); }

	bool checkStatus(AttackStatus status) override {
		checkedStatuses.push_back(status);
		if (alwaysStatus && status == *alwaysStatus && activations > 0) {
			--activations;
			return true;
		}
		return false;
	}

	void checkShield(const std::vector<Ptr<AttackResult>>& attackList, Ptr<Effect> effect, gameobjects::Creature& attacker) override {
		++shieldChecks;
		shieldListSize = attackList.size();
		AttackResult& result = *attackList.at(0);
		seenDamage = result.getDamage();
		seenStatus = result.getAttackStatus();
		seenHitType = result.getHitType();
		seenEffect = effect.get();
		seenAttacker = &attacker;
		if (shield) {
			result.setDamage(50);
			result.setReflectedDamage(5);
			result.setReflectedSkillId(111);
			result.setMpAbsorbed(3);
			result.setMpShieldSkillId(222);
			result.setProtectedDamage(4);
			result.setProtectedSkillId(333);
			result.setProtectorId(444);
			result.setShieldType(controllers::detail::shieldTypeId(skillengine::model::ShieldType::NORMAL));
			result.setLaunchSubEffect(false);
		}
	}

	Ref<controllers::observer::AttackerCriticalStatus> checkAttackerCriticalStatus(AttackStatus status, bool isSkill) override {
		criticalChecks.emplace_back(status, isSkill);
		if (!alwaysCritical)
			return controllers::observer::AttackerCriticalStatus::create(false);
		Ref<controllers::observer::AttackerCriticalStatus> acStatus = controllers::observer::AttackerCriticalStatus::create(1, 1000, false);
		acStatus->setResult(true);
		return acStatus;
	}

	float getBasePhysicalDamageMultiplier(bool isSkill) override {
		physicalMultiplierIsSkill.push_back(isSkill);
		return physicalMultiplier;
	}

	float getBaseMagicalDamageMultiplier() override { return magicalMultiplier; }

	std::optional<AttackStatus> alwaysStatus;
	int32_t activations = 0;
	bool shield = false;
	bool alwaysCritical = false;
	float physicalMultiplier = 1.0f;
	float magicalMultiplier = 1.0f;

	std::vector<AttackStatus> checkedStatuses;
	std::vector<std::pair<AttackStatus, bool>> criticalChecks;
	std::vector<bool> physicalMultiplierIsSkill;
	int32_t shieldChecks = 0;
	size_t shieldListSize = 0;
	int32_t seenDamage = -1;
	AttackStatus seenStatus = AttackStatus::DODGE;
	HitType seenHitType = HitType::EVERYHIT;
	const Effect* seenEffect = nullptr;
	const gameobjects::Creature* seenAttacker = nullptr;

protected:
	ProbeCalcObserver() = default;
	~ProbeCalcObserver() override = default;
};

/** A spawn template of the group, like the spawn data of a map */
class DamageSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	DamageSpawnTemplate(templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** GeneralInstanceHandler::getExpMultiplier is AION_UNPORTED; nothing here reads it, but WorldMap2DInstance needs a handler */
class DamageInstanceHandler final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit DamageInstanceHandler(world::WorldMapInstance& instance) : ::aion::gameserver::instance::handlers::GeneralInstanceHandler(instance) {}

	static Ref<DamageInstanceHandler> create(world::WorldMapInstance& instance) { return runtime::makeRef<DamageInstanceHandler>(instance); }

protected:
	~DamageInstanceHandler() override = default;
};

/** The SummonedObject containers stand in for the TrapGameStats/ServantGameStats that have no header yet (the pattern of MagicalCombatTest) */
class TestTrap final : public gameobjects::Trap {
	AION_MAKE_REF_FRIEND
public:
	TestTrap(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		gameobjects::Creature& creator)
		: Trap(key, std::move(controller), spawnTemplate, creator) {}

protected:
	~TestTrap() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

class TestServant final : public gameobjects::Servant {
	AION_MAKE_REF_FRIEND
public:
	TestServant(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate, int8_t level,
		gameobjects::Creature& creator)
		: Servant(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestServant() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

class TestSummonedObject final : public gameobjects::SummonedObject {
	AION_MAKE_REF_FRIEND
public:
	TestSummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		int8_t level, Ptr<gameobjects::VisibleObject> creator)
		: SummonedObject(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestSummonedObject() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

// ---- static data ----------------------------------------------------------------------------------------------------------------------------

constexpr int32_t POETA = 210010000;
constexpr int32_t DUMMY_NPC_ID = 700410;     // the target: level 1, PDef 100, MDef 200, MSup 100, strike resist 100
constexpr int32_t DUMMY_L4_NPC_ID = 700411;  // the same stats at level 4, three levels above a level-1 character
constexpr int32_t CASTER_NPC_ID = 700412;    // an npc attacker
constexpr int32_t TOTEM_NPC_ID = 700413;     // the template of the Trap, Servant and SummonedObject attackers

/** The first eleven rows of player_experience_table.xml, which PlayerCommonData.setLevel reads */
const char* const EXPERIENCE_TABLE_XML = R"(<player_experience_table>)"
										 R"(<exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp>)"
										 R"(<exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp>)"
										 R"(</player_experience_table>)";

/** Every stat NpcData would otherwise fill from NpcStatCalculation is written out (NpcData.cpp:64-90), so each derivation starts from a known one */
std::string npcTemplate(int32_t npcId, int32_t level, const char* name, const char* stats) {
	return std::string(R"(<npc_template npc_id=")") + std::to_string(npcId) + R"(" name_id="1" level=")" + std::to_string(level) + R"(" name=")" + name
		+ R"(" attack_speed="2000" rating="NORMAL" rank="NOVICE" tribe="MONSTER"><stats )" + stats
		+ R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)";
}

const char* const DUMMY_STATS = R"(maxHp="10000" maxMp="0" attack="16" matk="8" pdef="100" mdef="200" accuracy="60" pcrit="10" strike_resist="100")"
								R"( mresist="30" macc="20" mcrit="40" parry="50" msup="100")";
const char* const CASTER_STATS = R"(maxHp="10000" maxMp="0" attack="200" matk="300" pdef="50" mdef="40" accuracy="500" pcrit="10" mresist="30")"
								 R"( macc="20" mcrit="40" parry="50")";

std::string npcTemplatesXml() {
	return std::string("<npc_templates>") + npcTemplate(DUMMY_NPC_ID, 1, "dummy", DUMMY_STATS) + npcTemplate(DUMMY_L4_NPC_ID, 4, "dummy", DUMMY_STATS)
		+ npcTemplate(CASTER_NPC_ID, 1, "caster", CASTER_STATS) + npcTemplate(TOTEM_NPC_ID, 1, "totem", CASTER_STATS) + "</npc_templates>";
}

/** An active magical attack; apply_magical_skill_boost_bonus makes DamageEffect.shouldApplyMagicalSkillBoostBonus true */
const char* const MAGICAL_SKILL_XML = R"(<skill_template skill_id="9601" name="probe spell" nameId="1" stack="PROBE_SPELL" lvl="1")"
									  R"( skilltype="MAGICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0" apply_magical_skill_boost_bonus="true"/>)";
const char* const PHYSICAL_SKILL_XML = R"(<skill_template skill_id="9602" name="probe strike" nameId="1" stack="PROBE_STRIKE" lvl="1")"
									   R"( skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0"/>)";
/** The same spell with pvp_damage="50" (SkillTemplate.getPvpDamage, the percentage adjustDamageByPvpOrPveModifiers applies in PvP) */
const char* const PVP_SKILL_XML = R"(<skill_template skill_id="9603" name="probe pvp spell" nameId="1" stack="PROBE_PVP" lvl="1" skilltype="MAGICAL")"
								  R"( skillsubtype="ATTACK" activation="ACTIVE" duration="0" apply_magical_skill_boost_bonus="true" pvp_damage="50"/>)";

/** Binds XML text as T and keeps the object for the process (static data is immortal) */
template <class T>
const T* bindStatic(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<T>(context, xml).release();
}

/** The weapon-mastery skills Equipment.checkAvailableEquipSkills asks for: SWORD {37, 44}, SHIELD {43, 50}, ORB {111} (ItemGroupInfo.h) */
Ref<skill::PlayerSkillList> masterySkillList() {
	std::vector<Ref<skill::PlayerSkillEntry>> owned;
	std::vector<Ptr<skill::PlayerSkillEntry>> entries;
	for (int32_t skillId : {37, 39, 43, 44, 50, 100, 111}) {
		owned.push_back(skill::PlayerSkillEntry::create(skillId, 1, 0, gameobjects::Persistable::PersistentState::NOACTION));
		entries.emplace_back(*owned.back());
	}
	return skill::PlayerSkillList::create(entries);
}

// ---- fixture --------------------------------------------------------------------------------------------------------------------------------

class SkillDamageTest : public StatsPlayerTest {
protected:
	void SetUp() override {
		StatsPlayerTest::SetUp();
		publishMapStaticDataOnce();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcTemplatesXml()));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), EXPERIENCE_TABLE_XML));
		magicalSkill = bindSkill(MAGICAL_SKILL_XML);
		physicalSkill = bindSkill(PHYSICAL_SKILL_XML);
		pvpSkill = bindSkill(PVP_SKILL_XML);

		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return Ref<::aion::gameserver::instance::handlers::InstanceHandler>(DamageInstanceHandler::create(instance));
		});
	}

	void TearDown() override {
		mapInstance = nullptr;
		map = nullptr;
		groups.clear();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		StatsPlayerTest::TearDown();
	}

	const SkillTemplate* bindSkill(std::string_view xmlText) {
		skills.push_back(xml::bindString<SkillTemplate>(contexts.emplace_back(), xmlText));
		return skills.back().get();
	}

	templates::spawns::SpawnTemplate& makeSpawn(int32_t npcId, float x, float y, float z) {
		Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		templates::spawns::SpawnTemplate& spawnTemplate = group->addSpawnTemplate(std::make_unique<DamageSpawnTemplate>(*group, x, y, z));
		groups.push_back(group);
		return spawnTemplate;
	}

	/** places the object in the test map instance, so getPosition()->getWorldMapInstance() answers (Java: World.setPosition) */
	void place(gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(POETA, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true);
	}

	Ref<gameobjects::Npc> makeNpc(int32_t npcId, float x = 500, float y = 500, float z = 10) {
		Ref<gameobjects::Npc> npc = gameobjects::VisibleObject::create<gameobjects::Npc>(std::make_unique<controllers::NpcController>(),
			makeSpawn(npcId, x, y, z), dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		return npc;
	}

	/** A level-1 character; PlayerController.onLevelChange is what re-reads the class stats template, so the fixture does it by hand */
	PlayerFixture makeLevelOne(int32_t objectId, PlayerClass playerClass, float x = 501, float y = 500, float z = 10) {
		PlayerFixture fixture = makePlayer(objectId, playerClass);
		fixture.commonData->setLevel(1);
		fixture.player->getGameStats()->updateStatsTemplate();
		place(*fixture.player, x, y, z);
		fixture.player->setSkillList(masterySkillList());
		return fixture;
	}

	/** Equips a weapon or shield bound from `xml` into the slot (Equipment.onLoadHandler, the path a character's saved equipment takes) */
	void equip(gameobjects::player::Player& player, int32_t itemObjectId, std::string_view xml, items::ItemSlot slot) {
		Ref<gameobjects::Item> item =
			gameobjects::Item::create(itemObjectId, bindStatic<templates::item::ItemTemplate>(xml), 1, true, items::getSlotIdMask(slot));
		player.getEquipment().onLoadHandler(*item);
	}

	/** An orb (MAGICAL_FIRE, damage 40-60): PlayerGameStats.getMainHandMAttack answers its mean damage, 50, with no random draw */
	void equipOrb(gameobjects::player::Player& player, int32_t itemObjectId) {
		equip(player, itemObjectId,
			R"(<item_template id="100900001" level="1" item_group="ORB" attack_type="MAGICAL_FIRE"><weapon_stats hit_count="1")"
			R"( attack_range="1500" attack_speed="2000" max_damage="60" min_damage="40"/></item_template>)",
			items::ItemSlot::MAIN_HAND);
		ASSERT_TRUE(player.getEquipment().getMainHandWeapon()) << "the orb was not equipped";
	}

	/** A sword whose damage range is the single value 40, so PlayerGameStats.getMainHandPAttack's Rnd.get(min, max) always answers 40 */
	void equipSword(gameobjects::player::Player& player, int32_t itemObjectId) {
		equip(player, itemObjectId,
			R"(<item_template id="100000140" level="1" item_group="SWORD" attack_type="PHYSICAL"><weapon_stats hit_count="1")"
			R"( attack_range="1500" attack_speed="1400" max_damage="40" min_damage="40"/></item_template>)",
			items::ItemSlot::MAIN_HAND);
		ASSERT_TRUE(player.getEquipment().getMainHandWeapon()) << "the sword was not equipped";
	}

	/** A shield whose blocked damage is capped at 20 (WeaponStats.reduceMax, AttackUtil.calculateBlockedDamage) */
	void equipShield(gameobjects::player::Player& player, int32_t itemObjectId) {
		equip(player, itemObjectId, R"(<item_template id="115000140" level="1" item_group="SHIELD"><weapon_stats reduce_max="20"/></item_template>)",
			items::ItemSlot::SUB_HAND);
		ASSERT_TRUE(player.getEquipment().isShieldEquipped()) << "the shield was not equipped";
	}

	/** Adds `value` to the base of a stat of the creature (Java: an effect's StatAddFunction) */
	void addStat(gameobjects::Creature& creature, StatEnum stat, int32_t value) {
		addFunction(creature, RcStatFunction<StatAddFunction>::create(stat, value, false));
	}

	/** Adds a stat function of its own owner to the creature (Java: the functions an effect's StatOwner adds) */
	void addFunction(gameobjects::Creature& creature, const Ref<IStatFunction>& function) {
		Ref<TestStatOwner> owner = owners.emplace_back(TestStatOwner::create());
		creature.getGameStats()->addEffectOnly(Ptr<calc::StatOwner>(*owner), std::vector<Ptr<IStatFunction>>{Ptr<IStatFunction>(function)});
	}

	Ref<ProbeCalcObserver> observe(gameobjects::Creature& creature) {
		Ref<ProbeCalcObserver> observer = ProbeCalcObserver::create();
		creature.getObserveController()->addAttackCalcObserver(*observer);
		return observer;
	}

	/**
	 * An effect of the skill template at the level. `magicalCriticalAt` is the effect position whose magical critical the effect carries - Java's
	 * setMagicalCriticals stores index i for `positions.contains(i)` and isMagicalCritical(position) reads index position - 1 (Effect.java:298),
	 * so position p is passed as p - 1.
	 */
	Ref<Effect> effectOf(gameobjects::Creature& effector, gameobjects::Creature& effected, const SkillTemplate* skill, int32_t level = 1,
		std::optional<int32_t> magicalCriticalAt = std::nullopt) {
		std::unordered_set<int32_t> positions;
		if (magicalCriticalAt)
			positions.insert(*magicalCriticalAt - 1);
		return Effect::create(effector, Ptr<gameobjects::Creature>(effected), skill, level, std::nullopt, nullptr, false,
			magicalCriticalAt ? &positions : nullptr);
	}

	/** The reserved HP damage calculateEffectResult recorded for the position */
	static int32_t reservedDamage(Effect& effect, int32_t position = 1) { return effect.getReserveds(position)->getValue(); }

	std::deque<xml::LoadContext> contexts;
	std::vector<std::unique_ptr<SkillTemplate>> skills;
	std::vector<Ref<templates::spawns::SpawnGroup>> groups;
	std::vector<Ref<TestStatOwner>> owners;
	Ref<world::WorldMap> map;
	Ref<world::WorldMapInstance> mapInstance;
	const SkillTemplate* magicalSkill = nullptr;
	const SkillTemplate* physicalSkill = nullptr;
	const SkillTemplate* pvpSkill = nullptr;
};

// ------------------------------------------------------------------------------------------ StatFunctions.calculateMagicalSkillDamage

TEST_F(SkillDamageTest, MagicalSkillDamageGoldenVectors) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// A level-1 Mage (knowledge 115) with BOOST_MAGICAL_SKILL +600 and BOOST_SPELL_ATTACK +25 against the dummy (MSup 100, MDef 200) with
	// FIRE_RESISTANCE +325. 325 / 1300 is exactly 0.25, so the elemental factor is an exact 0.75 and every expected value below is exactly
	// representable in float (EXPECT_EQ, not EXPECT_FLOAT_EQ).
	PlayerFixture mage = makeLevelOne(9601, PlayerClass::MAGE);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	ASSERT_EQ(mage.player->getGameStats()->getKnowledge()->getCurrent(), 115) << "PlayerClass MAGE knowledge";
	ASSERT_EQ(mage.player->getGameStats()->getMBoost()->getCurrent(), 600);
	ASSERT_EQ(dummy->getGameStats()->getMBResist()->getCurrent(), 100) << R"(<stats msup="100">)";
	ASSERT_EQ(dummy->getGameStats()->getElementalDefenseFor(SkillElement::FIRE), 325);
	ASSERT_EQ(dummy->getGameStats()->getMDef()->getCurrent(), 200);

	const SpellProbe fire(SkillElement::FIRE, 1);
	const SpellProbe none(SkillElement::NONE, 1);
	const NoReduceProbe noReduce(SkillElement::FIRE, 1);
	auto damage = [&](gameobjects::Creature& effector, const skillengine::effect::EffectTemplate& template_, float base, int32_t bonus,
					  bool useMagicBoost, bool useKnowledge, bool useBoostSpellAttack) {
		return StatFunctions::calculateMagicalSkillDamage(effector, *dummy, base, bonus, &template_, useMagicBoost, useKnowledge, useBoostSpellAttack);
	};

	// every flag on:  magicBoost = (int) max(0, limit(2900, 600 - 100)) = 500
	//   100 * (500 / 1000f + 115 / 100f) = 100 * f(0.5 + 1.15f) = f(164.99999762) = 165
	//   BOOST_SPELL_ATTACK: getStat(BOOST_SPELL_ATTACK, (int) 165).getCurrent() = 165 + 25 = 190;  + bonus 7 = 197
	//   FIRE: 197 * (1 - 325 / 1300f) = 147.75;  MDef: 147.75 - 200 / 10f = 127.75
	EXPECT_EQ(damage(*mage.player, fire, 100, 7, true, true, true), 127.75f);
	// useMagicBoost false: magicBoost 0 -> 100 * 1.15f = f(114.99999762) = 115 -> 140 -> 147 -> 110.25 -> 90.25
	EXPECT_EQ(damage(*mage.player, fire, 100, 7, false, true, true), 90.25f);
	// useKnowledge false: knowledge 100 -> 100 * (0.5 + 1.0) = 150 -> 175 -> 182 -> 136.5 -> 116.5
	EXPECT_EQ(damage(*mage.player, fire, 100, 7, true, false, true), 116.5f);
	// useBoostSpellAttack false: 165 -> 172 -> 129 -> 109
	EXPECT_EQ(damage(*mage.player, fire, 100, 7, true, true, false), 109.0f);
	// SkillElement.NONE: neither the elemental defense nor the MDef tenth - 197
	EXPECT_EQ(damage(*mage.player, none, 100, 7, true, true, true), 197.0f);
	// NoReduceSpellATKInstantEffect with FIRE: `!(template instanceof NoReduceSpellATKInstantEffect)` skips both - 197
	EXPECT_EQ(damage(*mage.player, noReduce, 100, 7, true, true, true), 197.0f);
	// the floor: 1 * 1.0 = 1 -> + 0 -> 0.75 -> 0.75 - 20 = -19.25 -> 0
	EXPECT_EQ(damage(*mage.player, fire, 1, 0, false, false, false), 0.0f);

	// The rows above are exact in float, so they cannot see the operation order (m5b2-plan.md risk 11). These two are not:
	// - the factor is summed first: f(0.5 + 1.15f) = 1.64999998 exactly, 13 * 1.64999998 = 21.4499997 -> f = 21.4499989 (distributing the base,
	//   f(13 * 0.5) + f(13 * 1.15f) = 6.5 + 14.9499998 = f(21.4499998) = 21.4500008, is one float step higher)
	EXPECT_EQ(damage(*mage.player, none, 13, 0, true, true, false), 21.4499989f);
	// - the MDef tenth is a division: a dummy of MDef 203, flags off: 40 * 1.0 -> FIRE 30 -> 30 - f(203 / 10f) = 30 - 20.2999992 = 9.70000076
	//   (203 * 0.1f would be f(20.3000003) = 20.3000011 -> 9.69999886)
	Ref<gameobjects::Npc> mdef203 = makeNpc(DUMMY_NPC_ID, 503, 500, 10);
	addStat(*mdef203, StatEnum::MAGICAL_DEFEND, 3);
	addStat(*mdef203, StatEnum::FIRE_RESISTANCE, 325);
	EXPECT_EQ(StatFunctions::calculateMagicalSkillDamage(*mage.player, *mdef203, 40, 0, &fire, false, false, false), 9.70000076f);

	// limit(BOOST_MAGICAL_SKILL, 5100 - 100) = 2900 (StatCapUtil difference limit): 100 * f(2.9f + 1.15f) = f(405.00003) -> (int) 405 -> 430 ->
	// 437 -> 327.75 -> 307.75. Without the cap 5000 would give 100 * 6.15 = 615 -> 640 -> 647 -> 465.25.
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 4500);
	ASSERT_EQ(mage.player->getGameStats()->getMBoost()->getCurrent(), 5100);
	EXPECT_EQ(damage(*mage.player, fire, 100, 7, true, true, true), 307.75f);

	// a boost below the target's resist is 0, not negative: Math.max(0, 0 - 100) - the useMagicBoost false row's 90.25. A body without the max
	// would scale by -100 / 1000f + 1.15f = 1.05 and answer 105 -> 130 -> 137 -> 102.75 -> 82.75.
	PlayerFixture plainMage = makeLevelOne(9602, PlayerClass::MAGE, 502, 500, 10);
	addStat(*plainMage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	ASSERT_EQ(plainMage.player->getGameStats()->getMBoost()->getCurrent(), 0);
	EXPECT_EQ(damage(*plainMage.player, fire, 100, 7, true, true, true), 90.25f);

	// `getStat(BOOST_SPELL_ATTACK, (int) damage)`: the base is truncated before the stat functions see it. With the additive +25 above the cast
	// cannot be seen ((int) (x + 25) is (int) x + 25), and neither with a non-bonus PERCENT function (every BOOST_SPELL_ATTACK function of
	// skill_templates.xml), which truncates its base itself (StatRateFunction: getBaseWithoutBaseRate() * calculatePercent). A bonus PERCENT +10 %
	// shows it: 109.5 * 1.0 -> base (int) 109.5 = 109 -> bonus 109 * 10 / 100f = 10.9f -> (int) f(109 + 10.9f) = (int) 119.9 = 119. The
	// untruncated base 109.5 would give (int) 120.4 = 120.
	PlayerFixture rateMage = makeLevelOne(9603, PlayerClass::MAGE, 503, 500, 10);
	addFunction(*rateMage.player, RcStatFunction<StatRateFunction>::create(StatEnum::BOOST_SPELL_ATTACK, 10, true));
	EXPECT_EQ(damage(*rateMage.player, none, 109.5f, 0, false, false, true), 119.0f);
}

TEST_F(SkillDamageTest, AnNpcCastersMagicalSkillDamageVariesByEightPercentButASummonedObjectsDoesNot) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// StatFunctions.java:409-412: `effector instanceof Npc && !(effector instanceof SummonedObject)` adds Rnd.get(-rnd, rnd) with
	// rnd = (int) (damage * 0.08f). With every flag off and SkillElement.NONE the damage before it is the base damage itself: 1000 * (0 + 1.0)
	// = 1000, so rnd = (int) f(1000 * 0.08f) = (int) 80.0000012 = 80, and the answer is 1000 + the seed's first Rnd.get(-80, 80).
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	Ref<gameobjects::Npc> caster = makeNpc(CASTER_NPC_ID, 505, 500, 10);
	PlayerFixture master = makeLevelOne(9611, PlayerClass::MAGE);
	Ref<TestServant> servant = gameobjects::VisibleObject::create<TestServant>(std::make_unique<controllers::NpcController>(),
		makeSpawn(TOTEM_NPC_ID, 506, 500, 10), int8_t{1}, *master.player);
	place(*servant, 506, 500, 10);
	const SpellProbe none(SkillElement::NONE, 1);

	constexpr uint64_t SEED = 20260923;
	Rnd::seedCurrentThreadForTests(SEED);
	const int32_t roll = Rnd::get(-80, 80);
	ASSERT_NE(roll, 0) << "the seed must move the damage, or the case proves nothing";

	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_EQ(StatFunctions::calculateMagicalSkillDamage(*caster, *dummy, 1000, 0, &none, false, false, false), static_cast<float>(1000 + roll));
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_EQ(StatFunctions::calculateMagicalSkillDamage(*servant, *dummy, 1000, 0, &none, false, false, false), 1000.0f)
		<< "a Servant is a SummonedObject: no roll";
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_EQ(StatFunctions::calculateMagicalSkillDamage(*master.player, *dummy, 1000, 0, &none, false, false, false), 1000.0f)
		<< "and a character is no Npc";

	// rnd is taken from the float damage: 12.9f -> rnd = (int) f(12.9f * 0.08f) = (int) 1.03199995 = 1, so the answer is 12.9f + Rnd.get(-1, 1)
	// (from the truncated damage, (int) (12 * 0.08f) = 0, there would be no roll). The first seed whose Rnd.get(-1, 1) is not 0 is replayed.
	uint64_t nonZeroSeed = 1;
	int32_t smallRoll = 0;
	for (;; ++nonZeroSeed) {
		Rnd::seedCurrentThreadForTests(nonZeroSeed);
		smallRoll = Rnd::get(-1, 1);
		if (smallRoll != 0)
			break;
	}
	Rnd::seedCurrentThreadForTests(nonZeroSeed);
	EXPECT_EQ(StatFunctions::calculateMagicalSkillDamage(*caster, *dummy, 12.9f, 0, &none, false, false, false),
		12.9f + static_cast<float>(smallRoll))
		<< "seed " << nonZeroSeed;
}

// ----------------------------------------------------------------------------------- AttackUtil.calculateMagicalOverTimeSkillResult

TEST_F(SkillDamageTest, MagicalOverTimeGoldenVectors) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The Mage (+600 boost) ticks a FIRE dot on the dummy (FIRE_RESISTANCE +325, MSup 100, MDef 200). calculateMagicalSkillDamage runs with
	// useKnowledge and useBoostSpellAttack false (AttackUtil.java:436):
	//   useMagicBoost: magicBoost 500 -> 100 * (0.5 + 1.0) = 150 -> FIRE 112.5 -> MDef 92.5
	//   PvE: a character against a level-1 npc, level difference 0 -> * (1 - 0); PVE ratios 0 -> * (1 + 0 / 1000f) -> 92.5 -> (int) 92
	PlayerFixture mage = makeLevelOne(9621, PlayerClass::MAGE);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	SpellProbe fire(SkillElement::FIRE, 1);
	fire.critAddDmg2 = 30;
	fire.critAddDmg1 = 10;

	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 100, &fire, true), 92);
	// useMagicBoost false: 100 * 1.0 -> 75 -> 55
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 100, &fire, false), 55);
	// a magical critical at the template's position, skill level 2: critAddDmg = 30 + 10 * 2 = 50; calculateWeaponCritical with FIRE keeps 1.5
	// whatever the weapon, + 50 / 100f = 2.0 -> 92.5 * 2 = 185. Without the level term (30) it would be 1.8 -> 166.
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill, 2, 1), 100, &fire, true), 185);
	// the critical is the TEMPLATE's position: an effect critical at position 2 leaves this position-1 template's tick at 92
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill, 2, 2), 100, &fire, true), 92);
	// the floor at 1: 1 * 1.0 -> 0.75 - 20 < 0 -> calculateMagicalSkillDamage answers 0 -> `if (damage < 1) damage = 1`
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 1, &fire, false), 1);

	// the effector's observers multiply the magical damage (OneTimeBoostSkillAttackEffect): 92.5 * 3 = 277.5 -> 277
	Ref<ProbeCalcObserver> boost = observe(*mage.player);
	boost->magicalMultiplier = 3.0f;
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 100, &fire, true), 277);
	// the multiplier comes before the critical, and the order shows in the last float step: 64 * 1.5 = 96 -> 72 -> 52; multiplier 2.5 -> 130;
	// critical at level 1: critAddDmg 30 + 10 = 40 -> f(1.5 + 0.4f) = 1.89999998 -> f(130 * 1.89999998) = f(246.999997) = 247
	// (the critical first: f(52 * 1.89999998) = 98.7999954 -> * 2.5 = 246.999985 -> 246)
	boost->magicalMultiplier = 2.5f;
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill, 1, 1), 64, &fire, true), 247);
	mage.player->getObserveController()->removeAttackCalcObserver(*boost);

	// an npc target's AI has the last word (NpcAI.modifyDamage): 92.5 * 0.5 = 46.25 -> 46
	dummy->replaceAi(std::make_unique<ScalingNpcAI>(*dummy, 1.0f, 0.5f));
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 100, &fire, true), 46);
}

TEST_F(SkillDamageTest, ATrapsDotDealsItsTemplateDamage) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:432-433: `if (effector instanceof Trap) damage = skillDamage` - none of the magical arithmetic, so 33.9f -> (int) 33.
	// The same tick from the trap's creator (a Mage whose +600 boost would count) is 33.9 * (0.5 + 1.0) = 50.85 -> 38.1375 -> 18.1375 -> 18.
	PlayerFixture mage = makeLevelOne(9631, PlayerClass::MAGE);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	Ref<TestTrap> trap = gameobjects::VisibleObject::create<TestTrap>(std::make_unique<controllers::NpcController>(),
		makeSpawn(TOTEM_NPC_ID, 503, 500, 10), *mage.player);
	place(*trap, 503, 500, 10);
	const SpellProbe fire(SkillElement::FIRE, 1);

	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*trap, *dummy, magicalSkill), 33.9f, &fire, true), 33);
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 33.9f, &fire, true), 18);
}

TEST_F(SkillDamageTest, AMagicalDotTakesThePveRatiosOfItsElementAndAnElementlessCriticalTheWeaponsMultiplier) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// adjustDamageByPvpOrPveModifiers gets the template's element (AttackUtil.java:449). The Mage (+600 boost, PVE_ATTACK_RATIO_MAGICAL +50,
	// PVE_ATTACK_RATIO_PHYSICAL +500) ticks a FIRE dot on the level-4 dummy (FIRE_RESISTANCE +325, PVE_DEFEND_RATIO +100):
	//   calculateMagicalSkillDamage: 100 * 1.5 = 150 -> FIRE 112.5 -> MDef 92.5; multiplier 1; no critical
	//   PvE: level difference 3 -> * f(1 - 0.1f) = 0.9 -> f(92.5 * 0.9f) = 83.25; magical ratios 50 - 100 -> * f(1 - 0.05f) = 0.95
	//        -> f(83.25 * 0.95f) = 79.0875015 -> 79   (SkillElement.NONE would read the PHYSICAL ratio: 500 - 100 -> * 1.4 -> 116)
	PlayerFixture mage = makeLevelOne(9635, PlayerClass::MAGE);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_L4_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::PVE_ATTACK_RATIO_MAGICAL, 50);
	addStat(*mage.player, StatEnum::PVE_ATTACK_RATIO_PHYSICAL, 500);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	addStat(*dummy, StatEnum::PVE_DEFEND_RATIO, 100);
	const SpellProbe fire(SkillElement::FIRE, 1);
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *dummy, magicalSkill), 100, &fire, true), 79);

	// An element-less dot's critical takes the effector's weapon group (calculateWeaponCritical with SkillElement.NONE, AttackUtil.java:442):
	// a Warrior with a sword ticks 100 on the level-1 dummy - magic boost max(0, 0 - 100) = 0, knowledge 100 -> 100, no elemental reduction for
	// NONE -> critical: SWORD 2.2f + 0 / 100f -> f(100 * 2.2f) = 220 -> PvE * 1 -> 220 (no weapon group would keep 1.5: 150)
	PlayerFixture warrior = makeLevelOne(9636, PlayerClass::WARRIOR, 502, 500, 10);
	equipSword(*warrior.player, 9637);
	Ref<gameobjects::Npc> plainDummy = makeNpc(DUMMY_NPC_ID, 504, 500, 10);
	const SpellProbe none(SkillElement::NONE, 1);
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*warrior.player, *plainDummy, magicalSkill, 1, 1), 100, &none, true), 220);
}

TEST_F(SkillDamageTest, MagicalOverTimePvpGoldenVectors) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// A dot between two characters takes adjustDamageByPvpOrPveModifiers' PvP arm (StatFunctions.java:478-496) with the skill's pvp_damage 50.
	// The victim: FIRE_RESISTANCE +325 (a character's denominator is 1300 below level 51), MDef 0, MSup 0, PVP_DEFEND_RATIO +100 and
	// MAGICAL_CRITICAL_DAMAGE_REDUCE (fortitude) +100.
	//   calculateMagicalSkillDamage: magicBoost 600 - 0 -> 100 * f(0.6f + 1.0) = 160 -> FIRE 120 -> MDef 0 -> 120
	//   PvP: 120 * f(50 * 0.01f) = 120 * 0.5 = 60 -> * 0.42f = f(25.1999992) = 25.1999989
	//        multiplier 1 + (0 - 100) / 1000f = f(0.9) -> f(25.1999989 * 0.9f) = 22.6799984 -> (int) 22
	PlayerFixture mage = makeLevelOne(9641, PlayerClass::MAGE);
	PlayerFixture victim = makeLevelOne(9642, PlayerClass::MAGE, 503, 500, 10);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*victim.player, StatEnum::FIRE_RESISTANCE, 325);
	addStat(*victim.player, StatEnum::PVP_DEFEND_RATIO, 100);
	addStat(*victim.player, StatEnum::MAGICAL_CRITICAL_DAMAGE_REDUCE, 100);
	ASSERT_EQ(victim.player->getGameStats()->getMDef()->getCurrent(), 0);
	ASSERT_EQ(victim.player->getGameStats()->getMBResist()->getCurrent(), 0);
	SpellProbe fire(SkillElement::FIRE, 1);
	fire.critAddDmg2 = 50;

	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *victim.player, pvpSkill), 100, &fire, true), 22);
	// critical against a character: the fortitude (MAGICAL_CRITICAL_DAMAGE_REDUCE, AttackUtil.java:443) lowers the 1.5:
	//   f(1.5 - 100 / 1000f) = 1.4 -> + 50 / 100f = 1.9 -> 120 * 1.9 = 228 -> PvP 228 * 0.5 * 0.42f * 0.9f = 43.0919... -> 43
	// (without the fortitude: 120 * 2.0 = 240 -> 45)
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *victim.player, pvpSkill, 1, 1), 100, &fire, true), 43);
	// the floor at 1 comes after the PvP modifiers (AttackUtil.java:452): 1 * 1.6 -> FIRE 1.20000005 -> * 0.5 * 0.42f * 0.9 = 0.226799995 -> 1.
	// Floored before them, 1.2 would stay and the tick would be (int) 0.2268 = 0.
	EXPECT_EQ(AttackUtil::calculateMagicalOverTimeSkillResult(*effectOf(*mage.player, *victim.player, pvpSkill), 1, &fire, true), 1);

	// the same skill as an instant bolt through calculateSkillResult (orb, BOOST_SPELL_ATTACK +25, knowledge 115), which hands the skill's
	// pvp_damage to the PvP arm too: 141 * f(0.6f + 1.15f) = 141 * 1.75 = 246.75 -> (int) 246 + 25 = 271 -> FIRE 203.25 -> MDef 0
	//   -> * 0.5 = 101.625 -> * 0.42f = 42.6825 -> * 0.9 = 38.41 -> 38   (pvp_damage ignored: 76; the PvE arm instead: 203)
	equipOrb(*mage.player, 9643);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	Ref<Effect> bolt = effectOf(*mage.player, *victim.player, pvpSkill);
	AttackUtil::calculateSkillResult(*bolt, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*bolt), 38);
}

// ------------------------------------------------------------------------ AttackUtil.calculateSkillResult, the magical arm

TEST_F(SkillDamageTest, MagicalSkillGoldenVectorsOfALevelOneMageWithAnOrb) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The shape of 1282 Flame Bolt (`spellatkinstant value=141 element=FIRE`, m5b2-plan.md §2.4) from the Mage with an orb (MAttack base 50)
	// against the dummy. The Mage has BOOST_MAGICAL_SKILL +600 and BOOST_SPELL_ATTACK +25, the dummy FIRE_RESISTANCE +325.
	//   status: SkillElement FIRE -> effect.isMagicalCritical(1) -> false -> NORMALHIT; skill type MAGICAL -> HitType.MAHIT
	//   damage = 0 (no weapon attack for a magical element) + 141 (ADD)
	//   calculateMagicalSkillDamage(useMagicBoost = apply_magical_skill_boost_bonus, knowledge, boost spell):
	//     141 * f(0.5 + 1.15f) = f(232.649997) = 232.649994 -> (int) 232 + 25 = 257 -> + 0 -> FIRE 192.75 -> MDef 172.75
	//   movement: standing -> 172.75; one-time multiplier 1; PvE * 1 -> (int) 172
	PlayerFixture mage = makeLevelOne(9651, PlayerClass::MAGE);
	equipOrb(*mage.player, 9652);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	ASSERT_EQ(mage.player->getGameStats()->getMainHandMAttack({utils::stats::CalculationType::SKILL})->getBase(), 50);
	Ref<ProbeCalcObserver> seen = observe(*dummy);
	SpellProbe fire(SkillElement::FIRE, 1);
	fire.critAddDmg2 = 30;
	fire.critAddDmg1 = 10;

	Ref<Effect> bolt = effectOf(*mage.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*bolt, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*bolt), 172);
	EXPECT_EQ(bolt->getAttackStatus(), AttackStatus::NORMALHIT);
	EXPECT_EQ(seen->seenHitType, HitType::MAHIT) << "the hit type calculateEffectResult hands the shield observers";
	EXPECT_TRUE(bolt->getReserveds(1)->isSend());
	EXPECT_EQ(bolt->getEffectedHp(), 98) << "setReserveds(..., overTimeEffect false) computes the HP left: (int) (100f * 9828 / 10000)";

	// PERCENT: baseAttack * 200 / 100f = 50 * 2 = 100 -> 165 -> 190 -> 142.5 -> 122.5 -> 122
	fire.mode = skillengine::change::Func::PERCENT;
	Ref<Effect> percent = effectOf(*mage.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*percent, 200, &fire, false);
	EXPECT_EQ(reservedDamage(*percent), 122);
	// a PHYSICAL skill with an elemental template takes the default arm's magical attack too (AttackUtil.java:272): baseAttack 50 -> 100; the
	// physical skill has no apply_magical_skill_boost_bonus, so magicBoost 0: 100 * 1.15f = f(114.999998) = 115 -> 140 -> 105 -> 85
	// (the physical attack of an orb is 0: 0 -> 25 -> 18.75 - 20 -> 0)
	Ref<Effect> physicalPercent = effectOf(*mage.player, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*physicalPercent, 200, &fire, false);
	EXPECT_EQ(reservedDamage(*physicalPercent), 85);
	// `baseAttack * skillDamage` is an int product that wraps before the float division: 50 * 90000000 = 4500000000 -> 205032704 -> / 100f
	//   = f(2050327.04) = 2050327 -> * 1.64999998 = f(3383039.50) -> (int) 3383039 + 25 = 3383064 -> * 0.75 = 2537298 -> - 20 = 2537278
	// (a float product, f(50 * 9e7) / 100f = 44999996, would end at 55687492)
	Ref<Effect> wrapped = effectOf(*mage.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*wrapped, 90000000, &fire, false);
	EXPECT_EQ(reservedDamage(*wrapped), 2537278);
	fire.mode = skillengine::change::Func::ADD;

	// the action modifier's bonus is passed to calculateMagicalSkillDamage as `(int) bonus`, after the boost spell:
	//   ADD 30: 257 + 30 = 287 -> 215.25 -> 195.25 -> 195;  PERCENT 30: 50 * 30 / 100f = 15 -> 272 -> 204 -> 184
	fire.modifiers = modifiersOf(30, skillengine::change::Func::ADD);
	Ref<Effect> added = effectOf(*mage.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*added, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*added), 195);
	fire.modifiers = modifiersOf(30, skillengine::change::Func::PERCENT);
	Ref<Effect> scaled = effectOf(*mage.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*scaled, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*scaled), 184);
	fire.modifiers = nullptr;

	// the magical critical of the position, at skill level 2: critAddDmg 30 + 10 * 2 = 50 -> 172.75 * (1.5 + 0.5) = 345.5 -> 345
	Ref<Effect> critical = effectOf(*mage.player, *dummy, magicalSkill, 2, 1);
	AttackUtil::calculateSkillResult(*critical, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*critical), 345);
	EXPECT_EQ(critical->getAttackStatus(), AttackStatus::CRITICAL);
	EXPECT_EQ(critical->getReserveds(1)->getAttackStatus(), AttackStatus::CRITICAL);
}

TEST_F(SkillDamageTest, TheCastersMovementAndBoostMultiplyAMagicalSkillUnlessTheTemplateOptsOut) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The same bolt as MagicalSkillGoldenVectorsOfALevelOneMageWithAnOrb (172.75 before the multipliers):
	//   a OneTimeBoostSkillAttack-like observer (getBaseMagicalDamageMultiplier 1.5): 172.75 * 1.5 = 259.125 -> 259
	//   moving forward (adjustStatByMovementModifier MAGICAL_ATTACK * 1.1f): f(172.75 * 1.1f) = 190.025 -> 190
	//   both: 190.025 * 1.5 = 285.04 -> 285
	// ProcAtkInstantEffect answers false to shouldUseOneTimeBoostSkillAttack, shouldApplyAttackerMovementModifier and
	// shouldUseBoostSpellAttackEffects: 232.649994 + 0 (no BOOST_SPELL_ATTACK, and no (int) either) -> 174.4875 -> 154.4875 -> 154, and it is
	// not sent (`send = !(DelayedSpellAttackInstantEffect || ProcAtkInstantEffect)`).
	PlayerFixture mage = makeLevelOne(9661, PlayerClass::MAGE);
	equipOrb(*mage.player, 9662);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	const SpellProbe fire(SkillElement::FIRE, 1);
	const ProcAtkProbe proc(SkillElement::FIRE, 1);
	const DelayedProbe delayed(SkillElement::FIRE, 1);

	auto cast = [&](const skillengine::effect::DamageEffect& template_) {
		Ref<Effect> effect = effectOf(*mage.player, *dummy, magicalSkill);
		AttackUtil::calculateSkillResult(*effect, 141, &template_, false);
		return effect;
	};

	Ref<ProbeCalcObserver> boost = observe(*mage.player);
	boost->magicalMultiplier = 1.5f;
	EXPECT_EQ(reservedDamage(*cast(fire)), 259);

	mage.player->getObserveController()->removeAttackCalcObserver(*boost);
	Ptr<controllers::movement::PlayerMoveController> move = mage.player->getMoveController();
	move->setInMove(true); // getMovementDirection answers NONE while not moving and more than 1 s after the last move update
	move->setNewDirection(600, 500, 10); // heading 0 towards +x from (501, 500): a relative angle of 0 -> FORWARD
	ASSERT_EQ(move->getMovementDirection(), controllers::movement::PlayableMoveController::MovementModifierDirection::FORWARD);
	EXPECT_EQ(reservedDamage(*cast(fire)), 190);

	mage.player->getObserveController()->addAttackCalcObserver(*boost);
	EXPECT_EQ(reservedDamage(*cast(fire)), 285);

	Ref<Effect> procEffect = cast(proc);
	EXPECT_EQ(reservedDamage(*procEffect), 154) << "ProcAtkInstantEffect opts out of the boost, the movement and BOOST_SPELL_ATTACK";
	EXPECT_FALSE(procEffect->getReserveds(1)->isSend());
	Ref<Effect> delayedEffect = cast(delayed);
	EXPECT_EQ(reservedDamage(*delayedEffect), 285) << "DelayedSpellAttackInstantEffect keeps DamageEffect's answers";
	EXPECT_FALSE(delayedEffect->getReserveds(1)->isSend());
	EXPECT_TRUE(cast(fire)->getReserveds(1)->isSend());
	move->setInMove(false);
}

TEST_F(SkillDamageTest, NoReduceSpellAtkDealsItsTemplateDamageWithoutTheModifiers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// useTemplateDmg (`template instanceof NoReduceSpellATKInstantEffect`) skips the weapon and base attack reads, the magical arithmetic, the
	// critical and - through adjustDamageByPvpOrPveModifiers' useTemplateDmg - the level difference and the PvE ratios.
	// Against the level-4 dummy with PVE_DEFEND_RATIO +100, the Mage's PVE_ATTACK_RATIO_MAGICAL +50 and PVE_ATTACK_RATIO_PHYSICAL +500:
	//   the ordinary spell: 172.75 -> * f(1 - (4 - 1 - 2) * 0.1f) = 0.9 -> * f(1 + (50 - 100) / 1000f) = 0.95 -> 147.6... -> 147
	//   (a body that passed SkillElement.NONE instead of the template's element would read the PHYSICAL ratio: 1 + 400 / 1000f -> 217)
	//   NoReduceSpellATKInstant: 141, even with the magical critical of its position, whose status is still recorded
	PlayerFixture mage = makeLevelOne(9671, PlayerClass::MAGE);
	equipOrb(*mage.player, 9672);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_L4_NPC_ID);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	addStat(*mage.player, StatEnum::PVE_ATTACK_RATIO_MAGICAL, 50);
	addStat(*mage.player, StatEnum::PVE_ATTACK_RATIO_PHYSICAL, 500);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	addStat(*dummy, StatEnum::PVE_DEFEND_RATIO, 100);
	ASSERT_EQ(dummy->getLevel(), 4);
	const SpellProbe fire(SkillElement::FIRE, 1);
	const NoReduceProbe noReduce(SkillElement::FIRE, 1);

	Ref<Effect> bolt = effectOf(*mage.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*bolt, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*bolt), 147);

	Ref<Effect> fixed = effectOf(*mage.player, *dummy, magicalSkill, 1, 1);
	AttackUtil::calculateSkillResult(*fixed, 141, &noReduce, false);
	EXPECT_EQ(reservedDamage(*fixed), 141);
	EXPECT_EQ(fixed->getAttackStatus(), AttackStatus::CRITICAL);
}

TEST_F(SkillDamageTest, ASharedSkillDividesItsDamageAmongItsTargets) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:339-341: `effect.getSkill() != null && getEffectedList().size() > 1 && template.isShared()` -> damage / size.
	// The bolt of the golden vectors (172.75) on a skill with three targets: shared 172.75 / 3 = 57.58 -> 57, not shared 172.
	PlayerFixture mage = makeLevelOne(9681, PlayerClass::MAGE);
	equipOrb(*mage.player, 9682);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	Ref<gameobjects::Npc> second = makeNpc(DUMMY_NPC_ID, 510, 500, 10);
	Ref<gameobjects::Npc> third = makeNpc(DUMMY_NPC_ID, 511, 500, 10);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	Ref<skillengine::model::Skill> skill =
		skillengine::model::Skill::create(magicalSkill, *mage.player, 1, Ptr<gameobjects::Creature>(*dummy), nullptr);
	skill->getEffectedList().add(Ref<gameobjects::Creature>(*dummy));
	skill->getEffectedList().add(Ref<gameobjects::Creature>(*second));
	skill->getEffectedList().add(Ref<gameobjects::Creature>(*third));
	SpellProbe fire(SkillElement::FIRE, 1);

	fire.shared = true;
	Ref<Effect> shared = Effect::create(*skill, Ptr<gameobjects::Creature>(*dummy));
	AttackUtil::calculateSkillResult(*shared, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*shared), 57);

	fire.shared = false;
	Ref<Effect> single = Effect::create(*skill, Ptr<gameobjects::Creature>(*dummy));
	AttackUtil::calculateSkillResult(*single, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*single), 172);

	// The division comes before the PvE modifiers, and with a PvE multiplier the order shows: the dummy gets PVE_DEFEND_RATIO +100 (* 0.9f)
	// and the bolt 155 -> 155 * 1.64999998 = f(255.749996) = 255.75 -> (int) 255 + 25 = 280 -> 210 -> 190;
	// shared: f(190 / 3) = 63.3333321 -> f(63.3333321 * 0.9f) = 56.9999962 -> 56   (divided after: f(190 * 0.9f) = 171 -> 57)
	addStat(*dummy, StatEnum::PVE_DEFEND_RATIO, 100);
	fire.shared = true;
	Ref<Effect> sharedAfterPve = Effect::create(*skill, Ptr<gameobjects::Creature>(*dummy));
	AttackUtil::calculateSkillResult(*sharedAfterPve, 155, &fire, false);
	EXPECT_EQ(reservedDamage(*sharedAfterPve), 56);
}

// ------------------------------------------------------------------------ AttackUtil.calculateSkillResult, the physical arm

TEST_F(SkillDamageTest, PhysicalSkillGoldenVectorsOfALevelOneWarriorWithASword) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The shape of 2864 Ferocious Strike (`skillatk value=27`, m5b2-plan.md §2.4) from a Warrior whose sword always rolls 40, against the dummy.
	//   status: SkillElement NONE -> calculatePhysicalStatus: a skill never rolls the dodge, an npc never blocks or parries, and the dummy's
	//           strike resist 100 cancels the Warrior's critical 2 -> NORMALHIT
	//   skill type PHYSICAL, element NONE: baseAttack = getMainHandPAttack({SKILL, APPLY_POWER_SHARD_DAMAGE}).getBase() = 40, weapon attack
	//           calculateAttackDamage(NONE, {SKILL, ..., REMOVE_POWER_SHARD}) = [40.0] (the SKILL arm of a single weapon draws nothing else)
	//   damage = 40 + 27 (ADD) = 67; no bonus; one-time multiplier 1; standing; no critical
	//   PDef: 67 - 100 / 10 = 57; NORMALHIT; PvE * 1 -> 57
	PlayerFixture warrior = makeLevelOne(9701, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9702);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	ASSERT_EQ(dummy->getGameStats()->getPDef()->getCurrent(), 100);
	ASSERT_EQ(dummy->getGameStats()->getPCR()->getCurrent(), 100);
	Ref<ProbeCalcObserver> seen = observe(*dummy);
	SkillAttackProbe strike(SkillElement::NONE, 1);
	strike.critAddDmg2 = 30;

	auto strikeWith = [&](gameobjects::Creature& effector, gameobjects::Creature& effected, int32_t skillDamage) {
		Ref<Effect> effect = effectOf(effector, effected, physicalSkill);
		AttackUtil::calculateSkillResult(*effect, skillDamage, &strike, false);
		return effect;
	};

	Ref<Effect> hit = strikeWith(*warrior.player, *dummy, 27);
	EXPECT_EQ(reservedDamage(*hit), 57);
	EXPECT_EQ(hit->getAttackStatus(), AttackStatus::NORMALHIT);
	EXPECT_EQ(seen->seenHitType, HitType::PHHIT);

	// PERCENT: 40 + 40 * 150 / 100f = 100 -> 90
	strike.mode = skillengine::change::Func::PERCENT;
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 150)), 90);
	strike.mode = skillengine::change::Func::ADD;

	// `if (damage < 0) damage = 0` (AttackUtil.java:344): 40 - 100 - 10 = -70 -> 0
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, -100)), 0);

	// the action modifier's bonus is added to a physical hit before the multiplier: ADD 13 -> 80 -> 70; PERCENT 25 -> 40 * 25 / 100f = 10 -> 67
	strike.modifiers = modifiersOf(13, skillengine::change::Func::ADD);
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 27)), 70);
	strike.modifiers = modifiersOf(25, skillengine::change::Func::PERCENT);
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 27)), 67);
	strike.modifiers = nullptr;

	// rnddmg 4 multiplies by 1.0; an unknown rnddmg reaches randomizeDamage's IllegalArgumentException, which proves the template's value is read
	strike.rnddmg = 4;
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 27)), 57);
	strike.rnddmg = 11;
	EXPECT_THROW(strikeWith(*warrior.player, *dummy, 27), runtime::IllegalArgumentException);
	strike.rnddmg = 0;

	// a OneTimeBoostSkillAttack-like observer: getBasePhysicalDamageMultiplier(isSkill = true) 2 -> 134 -> 124
	Ref<ProbeCalcObserver> boost = observe(*warrior.player);
	boost->physicalMultiplier = 2.0f;
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 27)), 124);
	ASSERT_FALSE(boost->physicalMultiplierIsSkill.empty());
	EXPECT_TRUE(boost->physicalMultiplierIsSkill.back());
	boost->physicalMultiplier = 1.0f;

	// a certain critical (AttackerCriticalStatus, not percent, value 1000): the SWORD's 2.2f + critAddDmg 30 / 100f = f(2.5) -> 167.5 -> 157.5
	// -> 157 (1.5, the multiplier of no weapon, would give 110)
	boost->alwaysCritical = true;
	Ref<Effect> critical = strikeWith(*warrior.player, *dummy, 27);
	EXPECT_EQ(reservedDamage(*critical), 157);
	EXPECT_EQ(critical->getAttackStatus(), AttackStatus::CRITICAL);
	ASSERT_FALSE(boost->criticalChecks.empty());
	EXPECT_TRUE(boost->criticalChecks.back().second) << "calculatePhysicalStatus(template) passes isSkill = true";
	// rnddmg comes before the critical: rnddmg 3 (0.9f for a roll of 0-6), skill damage 8: f(48 * 0.9f) = 43.1999969 -> * 2.5 = 107.999992
	// -> - 10 = 97.9999924 -> 97   (the critical first: 48 * 2.5 = 120 -> f(120 * 0.9f) = 108 -> 98). The strike draws the certain critical's
	// Rnd.nextInt(1000), then randomizeDamage's Rnd.get(0, 19) (the sword's Rnd.get(40, 40) draws nothing); the first seed whose roll is 0-6
	// is replayed.
	uint64_t lowRollSeed = 1;
	for (;; ++lowRollSeed) {
		Rnd::seedCurrentThreadForTests(lowRollSeed);
		static_cast<void>(Rnd::nextInt(1000));
		if (Rnd::get(0, 19) <= 6)
			break;
	}
	strike.rnddmg = 3;
	Rnd::seedCurrentThreadForTests(lowRollSeed);
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 8)), 97) << "seed " << lowRollSeed;
	strike.rnddmg = 0;
	boost->alwaysCritical = false;

	// an npc target's AI: 57 * 0.5 = 28.5 -> 28
	dummy->replaceAi(std::make_unique<ScalingNpcAI>(*dummy, 1.0f, 0.5f));
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 27)), 28);
	dummy->replaceAi(std::make_unique<ScalingNpcAI>(*dummy, 1.0f, 1.0f));

	// the PDef tenth is a division (`def / 10`), and an npc that takes 15 times the damage shows it: PDef 104, skill damage 2:
	// 42 - f(104 / 10f) = 42 - 10.3999996 = 31.6000004 -> f(31.6000004 * 15) = 474   (104 * 0.1f = 10.4000006 -> 31.5999985 -> 473.999969 -> 473)
	Ref<gameobjects::Npc> pdef104 = makeNpc(DUMMY_NPC_ID, 503, 500, 10);
	addStat(*pdef104, StatEnum::PHYSICAL_DEFENSE, 4);
	pdef104->replaceAi(std::make_unique<ScalingNpcAI>(*pdef104, 1.0f, 15.0f));
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *pdef104, 2)), 474);

	// moving forward: PHYSICAL_ATTACK * 1.1f = f(73.7) -> 63.7 -> 63. Last, because the direction stays FORWARD for a second after the move
	// (PlayableMoveController.getMovementDirection) and a test cannot move the clock of the move controller.
	Ptr<controllers::movement::PlayerMoveController> move = warrior.player->getMoveController();
	move->setInMove(true);
	move->setNewDirection(600, 500, 10);
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 27)), 63);
	// the movement comes before the one-time multiplier: 40 + 110 = 150 -> f(150 * 1.1f) = 165 -> * 2.6f = f(428.999984) = 428.999969
	// -> - 10 = 418.999969 -> 418   (the multiplier first: f(150 * 2.6f) = 389.999985 -> * 1.1f = f(428.999993) = 429 -> 419)
	boost->physicalMultiplier = 2.6f;
	EXPECT_EQ(reservedDamage(*strikeWith(*warrior.player, *dummy, 110)), 418);
	boost->physicalMultiplier = 1.0f;
	move->setInMove(false);
}

TEST_F(SkillDamageTest, AnNpcCastersOwnAiScalesItsSkillDamage) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:335-337: `if (effector instanceof Npc) damage = effector.getAi().modifyOwnerDamage(damage, effected, effect)`. The weapon
	// attack of an npc rolls Rnd.get(80, 120), so the case uses an AI that answers 0: whatever the roll, the damage is 0 - and without the hook it
	// is 200 * roll / 100 + 27 - 10 > 0.
	Ref<gameobjects::Npc> caster = makeNpc(CASTER_NPC_ID, 505, 500, 10);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	const SkillAttackProbe strike(SkillElement::NONE, 1);

	Rnd::seedCurrentThreadForTests(4711);
	Ref<Effect> plain = effectOf(*caster, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*plain, 27, &strike, false);
	ASSERT_GT(reservedDamage(*plain), 150) << "the unscaled hit: at least 200 * 80 / 100 + 27 - 10";

	caster->replaceAi(std::make_unique<ScalingNpcAI>(*caster, 0.0f, 1.0f));
	Rnd::seedCurrentThreadForTests(4711);
	Ref<Effect> scaled = effectOf(*caster, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*scaled, 27, &strike, false);
	EXPECT_EQ(reservedDamage(*scaled), 0);
}

TEST_F(SkillDamageTest, AnNpcCastersSpellKeepsItsMagicalAttackAndItsAiScalesBeforeThePveModifiers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The dirty fix (AttackUtil.java:255) needs a magical attack of 0 AND a PHYSICAL attack type. An npc's attack type is PHYSICAL
	// (Npc.getAttackType) but its magical attack is its template's 300, so a MAGICAL, element-less PERCENT 150 skill keeps baseAttack 300 and
	// reads no weapon attack. Its status is physical: the npc's critical 10 against the strike resist 100 never lands.
	//   300 * 150 / 100f = 450 -> PDef 440   (with `||` the fix would add the weapon attack 2 * Rnd.get(80, 120) and switch baseAttack to the
	//   physical attack 200: 2 * roll + 300 - 10 >= 450)
	Ref<gameobjects::Npc> caster = makeNpc(CASTER_NPC_ID, 505, 500, 10);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	SpellProbe none(SkillElement::NONE, 1);
	none.mode = skillengine::change::Func::PERCENT;
	Ref<Effect> spell = effectOf(*caster, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*spell, 150, &none, false);
	EXPECT_EQ(reservedDamage(*spell), 440);

	// modifyOwnerDamage comes before the PvE modifiers (AttackUtil.java:335-342): the caster's AI deals 2.5 times its damage and the dummy has
	// PVE_DEFEND_RATIO +100 (* 0.9f). A physical strike of ADD 0 draws the critical roll, then the weapon attack's Rnd.get(80, 120); the first
	// seed whose weapon roll is 101 is replayed:
	//   200 * 101 / 100f = 202 -> PDef 192 -> * 2.5 = 480 -> f(480 * 0.9f) = f(431.999989) = 432
	//   (the PvE multiplier first: f(192 * 0.9f) = 172.799988 -> * 2.5 = 431.999969 -> 431)
	caster->replaceAi(std::make_unique<ScalingNpcAI>(*caster, 2.5f, 1.0f));
	addStat(*dummy, StatEnum::PVE_DEFEND_RATIO, 100);
	uint64_t seed = 1;
	for (;; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		static_cast<void>(Rnd::nextInt(1000));
		if (Rnd::get(80, 120) == 101)
			break;
	}
	const SkillAttackProbe strike(SkillElement::NONE, 1);
	Rnd::seedCurrentThreadForTests(seed);
	Ref<Effect> hit = effectOf(*caster, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*hit, 0, &strike, false);
	EXPECT_EQ(reservedDamage(*hit), 432) << "seed " << seed;
}

TEST_F(SkillDamageTest, APhysicalSkillAgainstACharacterIsBlockedParriedAndCritInPvp) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Character against character: PvP (* 0.42f, pvp_damage 0 skips the percentage), and the block and parry of calculatePhysicalStatus. The
	// victims carry PHYSICAL_CRITICAL_RESIST +100, so only an observer can make a hit critical, and PHYSICAL_DEFENSE +100 on a character's PDef
	// of 0, so the PDef tenth (67 - 100 / 10 = 57) comes before the block and the parry (AttackUtil.java:320-329).
	//   block (shield reduce_max 20, DAMAGE_REDUCE +40 -> the reverse stat 100 - 40 = 60):
	//     calculateBlockedDamage: reduceVal = 57 - f(57 * 60) / 100 = 57 - 34.2 = 22.8 > reduceMax 20 -> 20; 57 - 20 = 37
	//     -> f(37 * 0.42f) = 15.54 -> 15
	//   block below the cap (DAMAGE_REDUCE +20 -> 80): reduceVal = 57 - f(4560 / 100) = 57 - 45.5999985 = 11.4000015 -> 45.5999985
	//     -> * 0.42f = 19.1519985 -> 19   (blocked before the PDef tenth: 67 - 13.4000015 = 53.5999985 - 10 -> 18.3119984 -> 18)
	//   parry: f(57 * 0.6f) = 34.2000008 -> * 0.42f = 14.3640003 -> 14   (parried before the PDef tenth: 40.2 - 10 -> 12.684 -> 12)
	//   critical parry, fortitude PHYSICAL_CRITICAL_DAMAGE_REDUCE +200: f(2.2f - 0.2f) = 2.0 -> + 0.3 = 2.3 -> f(67 * 2.3f) = 154.099991
	//     -> - 10 = 144.099991 -> * 0.6f = 86.4599991 -> * 0.42f = 36.3131981 -> 36   (the MAGICAL fortitude stat would leave 2.5 -> 39;
	//     parried before the PDef tenth: 92.4599991 - 10 -> 34.6331978 -> 34)
	PlayerFixture warrior = makeLevelOne(9711, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9712);
	PlayerFixture shieldBearer = makeLevelOne(9713, PlayerClass::WARRIOR, 503, 500, 10);
	equipShield(*shieldBearer.player, 9714);
	PlayerFixture parrier = makeLevelOne(9715, PlayerClass::WARRIOR, 504, 500, 10);
	PlayerFixture lightBlocker = makeLevelOne(9716, PlayerClass::WARRIOR, 505, 500, 10);
	equipShield(*lightBlocker.player, 9717);
	for (gameobjects::player::Player* victim : {shieldBearer.player.get(), parrier.player.get(), lightBlocker.player.get()}) {
		addStat(*victim, StatEnum::PHYSICAL_CRITICAL_RESIST, 100);
		ASSERT_EQ(victim->getGameStats()->getPDef()->getCurrent(), 0);
		addStat(*victim, StatEnum::PHYSICAL_DEFENSE, 100);
	}
	addStat(*shieldBearer.player, StatEnum::DAMAGE_REDUCE, 40);
	ASSERT_EQ(shieldBearer.player->getGameStats()->getReverseStat(StatEnum::DAMAGE_REDUCE, 100)->getCurrent(), 60);
	addStat(*lightBlocker.player, StatEnum::DAMAGE_REDUCE, 20);
	addStat(*parrier.player, StatEnum::PHYSICAL_CRITICAL_DAMAGE_REDUCE, 200);
	Ref<ProbeCalcObserver> alwaysBlock = observe(*shieldBearer.player);
	alwaysBlock->alwaysStatus = AttackStatus::BLOCK;
	alwaysBlock->activations = 100;
	Ref<ProbeCalcObserver> alwaysLightBlock = observe(*lightBlocker.player);
	alwaysLightBlock->alwaysStatus = AttackStatus::BLOCK;
	alwaysLightBlock->activations = 100;
	Ref<ProbeCalcObserver> alwaysParry = observe(*parrier.player);
	alwaysParry->alwaysStatus = AttackStatus::PARRY;
	alwaysParry->activations = 100;
	SkillAttackProbe strike(SkillElement::NONE, 1);
	strike.critAddDmg2 = 30;

	Ref<Effect> blocked = effectOf(*warrior.player, *shieldBearer.player, physicalSkill);
	AttackUtil::calculateSkillResult(*blocked, 27, &strike, false);
	EXPECT_EQ(blocked->getAttackStatus(), AttackStatus::BLOCK);
	EXPECT_EQ(reservedDamage(*blocked), 15);

	Ref<Effect> lightlyBlocked = effectOf(*warrior.player, *lightBlocker.player, physicalSkill);
	AttackUtil::calculateSkillResult(*lightlyBlocked, 27, &strike, false);
	EXPECT_EQ(reservedDamage(*lightlyBlocked), 19);

	Ref<Effect> parried = effectOf(*warrior.player, *parrier.player, physicalSkill);
	AttackUtil::calculateSkillResult(*parried, 27, &strike, false);
	EXPECT_EQ(parried->getAttackStatus(), AttackStatus::PARRY);
	EXPECT_EQ(reservedDamage(*parried), 14);

	Ref<ProbeCalcObserver> certainCritical = observe(*warrior.player);
	certainCritical->alwaysCritical = true;
	Ref<Effect> criticalParry = effectOf(*warrior.player, *parrier.player, physicalSkill);
	AttackUtil::calculateSkillResult(*criticalParry, 27, &strike, false);
	EXPECT_EQ(criticalParry->getAttackStatus(), AttackStatus::CRITICAL_PARRY);
	EXPECT_EQ(reservedDamage(*criticalParry), 36);
}

TEST_F(SkillDamageTest, AVictimMovingForwardLowersItsPhysicalAndMagicalDefense) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The target's defences pass adjustStatByMovementModifier too (AttackUtil.java:318, StatFunctions.java:402): a character running FORWARD
	// keeps 80 % of its PDef and MDef and 50 less elemental defense. The runner has PHYSICAL_DEFENSE +100, MAGICAL_DEFEND +200,
	// FIRE_RESISTANCE +375 and PHYSICAL_CRITICAL_RESIST +100 (no critical); its parry 74 is far below the Warrior's accuracy 198.
	//   the Warrior's strike: 67 - f(100 * 0.8f) / 10 = 67 - 8 = 59 -> PvP * 0.42f = 24.7799988 -> 24   (the unmoved PDef: 57 -> 23)
	//   the Mage's bolt (orb, +600 boost, BOOST_SPELL_ATTACK +25): 141 * f(0.6f + 1.15f) = 141 * 1.75 = 246.75 -> 246 + 25 = 271
	//     -> FIRE (int) (375 - 50) = 325 -> * 0.75 = 203.25 -> MDef f(200 * 0.8f) = 160 -> - 16 = 187.25 -> PvP * 0.42f = 78.6449966 -> 78
	//     (the unmoved MDef: 183.25 -> 76)
	PlayerFixture warrior = makeLevelOne(9791, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9792);
	PlayerFixture mage = makeLevelOne(9793, PlayerClass::MAGE, 502, 500, 10);
	equipOrb(*mage.player, 9794);
	addStat(*mage.player, StatEnum::BOOST_MAGICAL_SKILL, 600);
	addStat(*mage.player, StatEnum::BOOST_SPELL_ATTACK, 25);
	PlayerFixture runner = makeLevelOne(9795, PlayerClass::WARRIOR, 503, 500, 10);
	addStat(*runner.player, StatEnum::PHYSICAL_DEFENSE, 100);
	addStat(*runner.player, StatEnum::MAGICAL_DEFEND, 200);
	addStat(*runner.player, StatEnum::FIRE_RESISTANCE, 375);
	addStat(*runner.player, StatEnum::PHYSICAL_CRITICAL_RESIST, 100);
	Ptr<controllers::movement::PlayerMoveController> move = runner.player->getMoveController();
	move->setInMove(true);
	move->setNewDirection(600, 500, 10); // heading 0 towards +x from (503, 500): FORWARD
	const SkillAttackProbe strike(SkillElement::NONE, 1);
	const SpellProbe fire(SkillElement::FIRE, 1);

	Ref<Effect> hit = effectOf(*warrior.player, *runner.player, physicalSkill);
	AttackUtil::calculateSkillResult(*hit, 27, &strike, false);
	EXPECT_EQ(reservedDamage(*hit), 24);
	Ref<Effect> bolt = effectOf(*mage.player, *runner.player, magicalSkill);
	AttackUtil::calculateSkillResult(*bolt, 141, &fire, false);
	EXPECT_EQ(reservedDamage(*bolt), 78);
	move->setInMove(false);
}

TEST_F(SkillDamageTest, AMagicalSkillCastWithAPhysicalWeaponUsesThePhysicalAttack) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// The "dirty fix for staffs and maces" (AttackUtil.java:255-262): a MAGICAL skill whose effector has no magical attack (a sword answers an
	// AdditionStat of 0) and a PHYSICAL attack type reads the physical attack instead - and, for SkillElement.NONE, the weapon's damage too.
	// PERCENT 150 with element NONE: weapon attack [40] + baseAttack 40 * 150 / 100f = 100 -> physical: 100 - 10 = 90, hit type MAHIT.
	// Without the fix: baseAttack 0 and no weapon attack -> 0 - 10 -> 0.
	PlayerFixture warrior = makeLevelOne(9721, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9722);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	ASSERT_EQ(warrior.player->getGameStats()->getMainHandMAttack({utils::stats::CalculationType::SKILL})->getBase(), 0);
	Ref<ProbeCalcObserver> seen = observe(*dummy);
	SkillAttackProbe strike(SkillElement::NONE, 1);
	strike.mode = skillengine::change::Func::PERCENT;

	Ref<Effect> effect = effectOf(*warrior.player, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*effect, 150, &strike, false);
	EXPECT_EQ(reservedDamage(*effect), 90);
	EXPECT_EQ(seen->seenHitType, HitType::MAHIT);
}

TEST_F(SkillDamageTest, ASummonedObjectAttacksWithItsPhysicalAttackButAServantDoesNot) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:246-249: a SummonedObject that is no Servant takes its hit type from the skill type and ALWAYS adds its physical weapon
	// attack, whatever the element. A FIRE skill of ADD 0 from the totem template (attack 200, magic boost and knowledge of an npc):
	//   weapon attack = 200 * roll / 100f with roll = Rnd.get(80, 120), the only draw of the call (a magical status rolls nothing, and a
	//   SummonedObject skips calculateMagicalSkillDamage's +-8 %) = 2 * roll
	//   calculateMagicalSkillDamage: magicBoost max(0, 0 - 100) = 0, knowledge 100 -> 2 * roll -> FIRE 0.75 -> - 20 -> 1.5 * roll - 20
	// A Servant (Java: `!(effector instanceof Servant)`) takes the ordinary arm: no weapon attack, 0 + 0 -> 0 - 20 -> 0.
	PlayerFixture master = makeLevelOne(9731, PlayerClass::MAGE);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	addStat(*dummy, StatEnum::FIRE_RESISTANCE, 325);
	Ref<ProbeCalcObserver> seen = observe(*dummy);
	Ref<gameobjects::Npc> creator = makeNpc(CASTER_NPC_ID, 505, 500, 10);
	Ref<TestSummonedObject> summoned = gameobjects::VisibleObject::create<TestSummonedObject>(std::make_unique<controllers::NpcController>(),
		makeSpawn(TOTEM_NPC_ID, 506, 500, 10), int8_t{1}, Ptr<gameobjects::VisibleObject>(*creator));
	place(*summoned, 506, 500, 10);
	Ref<TestServant> servant = gameobjects::VisibleObject::create<TestServant>(std::make_unique<controllers::NpcController>(),
		makeSpawn(TOTEM_NPC_ID, 507, 500, 10), int8_t{1}, *master.player);
	place(*servant, 507, 500, 10);
	const SpellProbe fire(SkillElement::FIRE, 1);

	constexpr uint64_t SEED = 31337;
	Rnd::seedCurrentThreadForTests(SEED);
	const int32_t roll = Rnd::get(80, 120);
	Rnd::seedCurrentThreadForTests(SEED);
	Ref<Effect> fromSummoned = effectOf(*summoned, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*fromSummoned, 0, &fire, false);
	EXPECT_EQ(reservedDamage(*fromSummoned), static_cast<int32_t>(1.5f * static_cast<float>(roll) - 20)) << "roll " << roll;
	EXPECT_EQ(seen->seenHitType, HitType::MAHIT) << "a MAGICAL skill";
	Ref<Effect> physical = effectOf(*summoned, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*physical, 0, &fire, false);
	EXPECT_EQ(seen->seenHitType, HitType::PHHIT) << "a PHYSICAL skill, whatever the element";
	// its baseAttack is the physical attack's base 200 too, which a PERCENT skill reads: PERCENT 50 -> 2 * roll + 200 * 50 / 100f = 2 * roll + 100
	// -> * 0.75 - 20 = 1.5 * roll + 55   (a baseAttack of 0 would leave 1.5 * roll - 20)
	SpellProbe percentFire(SkillElement::FIRE, 1);
	percentFire.mode = skillengine::change::Func::PERCENT;
	Rnd::seedCurrentThreadForTests(SEED);
	Ref<Effect> percentFromSummoned = effectOf(*summoned, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*percentFromSummoned, 50, &percentFire, false);
	EXPECT_EQ(reservedDamage(*percentFromSummoned), static_cast<int32_t>(static_cast<float>(2 * roll + 100) * 0.75f - 20)) << "roll " << roll;

	Ref<Effect> fromServant = effectOf(*servant, *dummy, magicalSkill);
	AttackUtil::calculateSkillResult(*fromServant, 0, &fire, false);
	EXPECT_EQ(reservedDamage(*fromServant), 0);
}

// -------------------------------------------------------------- AttackUtil.calculatePhysicalStatus(attacker, attacked, template, effect)

TEST_F(SkillDamageTest, CannotMissSkipsBlockParryAndDodgeButStillConsumesTheirActivations) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:459 and :475-479: SkillAttackInstantEffect.isCannotmiss makes calculatePhysicalStatus call the three checks for their side
	// effect only. The shield bearer's always-block observer is asked DODGE, BLOCK and PARRY (in that order), spends its activation on the BLOCK
	// question, and the hit lands anyway. Without cannotmiss a skill asks BLOCK only (no dodge for a skill, and a block ends the chain).
	PlayerFixture warrior = makeLevelOne(9741, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9742);
	PlayerFixture shieldBearer = makeLevelOne(9743, PlayerClass::WARRIOR, 503, 500, 10);
	equipShield(*shieldBearer.player, 9744);
	addStat(*shieldBearer.player, StatEnum::PHYSICAL_CRITICAL_RESIST, 100);
	Ref<ProbeCalcObserver> alwaysBlock = observe(*shieldBearer.player);
	alwaysBlock->alwaysStatus = AttackStatus::BLOCK;
	SkillAttackProbe strike(SkillElement::NONE, 1);
	const auto physicalStatus = privateMember(PhysicalStatusOfTemplateTag{});

	strike.cannotmiss = true;
	alwaysBlock->activations = 1;
	Ref<Effect> effect = effectOf(*warrior.player, *shieldBearer.player, physicalSkill);
	EXPECT_EQ(physicalStatus(*warrior.player, *shieldBearer.player, &strike, *effect), AttackStatus::NORMALHIT);
	EXPECT_EQ(alwaysBlock->checkedStatuses, (std::vector<AttackStatus>{AttackStatus::DODGE, AttackStatus::BLOCK, AttackStatus::PARRY}));
	EXPECT_EQ(alwaysBlock->activations, 0) << "the always-block activation is spent although the hit was not blocked";

	strike.cannotmiss = false;
	alwaysBlock->activations = 1;
	alwaysBlock->checkedStatuses.clear();
	EXPECT_EQ(physicalStatus(*warrior.player, *shieldBearer.player, &strike, *effect), AttackStatus::BLOCK);
	EXPECT_EQ(alwaysBlock->checkedStatuses, (std::vector<AttackStatus>{AttackStatus::BLOCK}));
}

TEST_F(SkillDamageTest, TheTemplatesAccuracyModifierIsAccMod2PlusAccMod1TimesTheSkillLevel) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:458: accMod = accMod2 + accMod1 * level, subtracted from the victim's parry rate by checkIsParriedHit:
	// limit(PARRY, parry - (accuracy + accMod)). The parrier has parry 74 + 524 = 598 and the Warrior accuracy 198, so the rate is 400 - accMod
	// (400 is also PARRY's difference limit). With accMod2 100 and accMod1 50:
	//   level 2: 100 + 50 * 2 = 200 -> rate 200   (accMod1 + accMod2 * 2 = 250 -> 150; (accMod2 + accMod1) * 2 = 300 -> 100;
	//            accMod2 alone -> 300; accMod2 - accMod1 * 2 = 0 -> 400)
	//   level 1: 100 + 50 * 1 = 150 -> rate 250   (accMod2 alone -> 300)
	// Each call draws Rnd.nextInt(1000) twice - the parry roll, parried below the rate, and the critical roll (the Warrior's critical 2 against
	// PHYSICAL_CRITICAL_RESIST 100 never lands, but is rolled) - so the expected count replays the seed's stream at the expected rate.
	PlayerFixture warrior = makeLevelOne(9751, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9752);
	PlayerFixture parrier = makeLevelOne(9753, PlayerClass::WARRIOR, 503, 500, 10);
	addStat(*parrier.player, StatEnum::PARRY, 524);
	addStat(*parrier.player, StatEnum::PHYSICAL_CRITICAL_RESIST, 100);
	ASSERT_EQ(parrier.player->getGameStats()->getParry()->getCurrent(), 598);
	ASSERT_EQ(warrior.player->getGameStats()->getMainHandPAccuracy()->getCurrent(), 198);
	SkillAttackProbe strike(SkillElement::NONE, 1);
	const auto physicalStatus = privateMember(PhysicalStatusOfTemplateTag{});
	constexpr uint64_t SEED = 20260923;
	constexpr int32_t HITS = 400;

	auto parries = [&](int32_t level) {
		Ref<Effect> effect = effectOf(*warrior.player, *parrier.player, physicalSkill, level);
		Rnd::seedCurrentThreadForTests(SEED);
		int32_t count = 0;
		for (int32_t i = 0; i < HITS; i++)
			count += physicalStatus(*warrior.player, *parrier.player, &strike, *effect) == AttackStatus::PARRY ? 1 : 0;
		return count;
	};
	auto replayed = [&](int32_t rate) {
		Rnd::seedCurrentThreadForTests(SEED);
		int32_t count = 0;
		for (int32_t i = 0; i < HITS; i++) {
			const int32_t parryRoll = Rnd::nextInt(1000);
			static_cast<void>(Rnd::nextInt(1000)); // the critical roll
			count += parryRoll < rate ? 1 : 0;
		}
		return count;
	};

	EXPECT_EQ(parries(2), replayed(400)) << "no modifier: the whole rate";
	strike.accMod2 = 100;
	strike.accMod1 = 50;
	EXPECT_EQ(parries(2), replayed(200)) << "100 + 50 * 2";
	EXPECT_EQ(parries(1), replayed(250)) << "100 + 50 * 1";
}

TEST_F(SkillDamageTest, TheTemplatesCriticalProbabilityIsCritProbMod2PlusCritProbMod1TimesTheSkillLevel) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:460 passes template.calculateCritProbMod(effect) = critProbMod2 + critProbMod1 * level as checkIsPhysicalCriticalHit's
	// criticalProb, which scales a positive rate by criticalProb / 100f. The Warrior's critical 2 + 300 against the dummy's strike resist 100 is a
	// rate of 202: criticalProb 0 scales it to nothing, criticalProb 0 + 50 * 2 = 100 leaves it whole.
	PlayerFixture warrior = makeLevelOne(9761, PlayerClass::WARRIOR);
	equipSword(*warrior.player, 9762);
	addStat(*warrior.player, StatEnum::PHYSICAL_CRITICAL, 300);
	ASSERT_EQ(warrior.player->getGameStats()->getMainHandPCritical()->getCurrent(), 302);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	SkillAttackProbe strike(SkillElement::NONE, 1);
	const auto physicalStatus = privateMember(PhysicalStatusOfTemplateTag{});
	Ref<Effect> effect = effectOf(*warrior.player, *dummy, physicalSkill, 2);

	auto criticals = [&]() {
		Rnd::seedCurrentThreadForTests(4711);
		int32_t count = 0;
		for (int32_t i = 0; i < 400; i++)
			count += physicalStatus(*warrior.player, *dummy, &strike, *effect) == AttackStatus::CRITICAL ? 1 : 0;
		return count;
	};

	strike.critProbMod2 = 0;
	EXPECT_EQ(criticals(), 0) << "criticalProb 0";
	strike.critProbMod1 = 50;
	EXPECT_GT(criticals(), 40) << "criticalProb 0 + 50 * 2 = 100: 202 of 1000";
}

// --------------------------------------------------------------------------------------------------- AttackUtil.calculateEffectResult

TEST_F(SkillDamageTest, TheShieldObserversResultIsCopiedIntoTheEffect) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// AttackUtil.java:389-403. The observer is handed a one-element list holding AttackResult(80, CRITICAL, MAHIT), the effect and the effector;
	// every field it writes lands in the Effect, the reserved damage is the shielded 50 at the position, and launchSubEffect follows the result.
	PlayerFixture mage = makeLevelOne(9771, PlayerClass::MAGE);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	Ref<ProbeCalcObserver> shield = observe(*dummy);
	shield->shield = true;
	const auto effectResult = privateMember(EffectResultTag{});

	Ref<Effect> effect = effectOf(*mage.player, *dummy, magicalSkill);
	effectResult(*effect, *dummy, 80, AttackStatus::CRITICAL, HitType::MAHIT, false, 2, true);
	EXPECT_EQ(shield->shieldChecks, 1);
	EXPECT_EQ(shield->shieldListSize, 1u);
	EXPECT_EQ(shield->seenDamage, 80);
	EXPECT_EQ(shield->seenStatus, AttackStatus::CRITICAL);
	EXPECT_EQ(shield->seenHitType, HitType::MAHIT);
	EXPECT_EQ(shield->seenEffect, effect.get());
	EXPECT_EQ(shield->seenAttacker, mage.player.get());
	EXPECT_EQ(effect->getReflectedDamage(), 5);
	EXPECT_EQ(effect->getReflectedSkillId(), 111);
	EXPECT_EQ(effect->getMpAbsorbed(), 3);
	EXPECT_EQ(effect->getMpShieldSkillId(), 222);
	EXPECT_EQ(effect->getProtectedDamage(), 4);
	EXPECT_EQ(effect->getProtectedSkillId(), 333);
	EXPECT_EQ(effect->getProtectorId(), 444);
	EXPECT_EQ(effect->getShieldDefense(), controllers::detail::shieldTypeId(skillengine::model::ShieldType::NORMAL));
	EXPECT_EQ(reservedDamage(*effect, 2), 50);
	EXPECT_TRUE(effect->getReserveds(2)->isSend());
	EXPECT_TRUE(effect->getReserveds(2)->isDamage());
	EXPECT_EQ(effect->getReserveds(2)->getAttackStatus(), AttackStatus::CRITICAL);
	EXPECT_EQ(effect->getAttackStatus(), AttackStatus::CRITICAL);
	EXPECT_FALSE(effect->isLaunchSubEffect());
}

TEST_F(SkillDamageTest, IgnoreShieldBypassesTheShieldObservers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// `if (!ignoreShield)` (AttackUtil.java:390): the observer is never asked, nothing is copied, and the reserved damage is the unshielded one.
	// calculateSkillResult passes its own ignoreShield through (DelayedSpellAttackInstantEffect.calculateDamage passes true, "ignores shields on
	// retail", DelayedSpellAttackInstantEffect.java:28; every other caller false) together with the template's position and the `send` flag.
	PlayerFixture mage = makeLevelOne(9781, PlayerClass::MAGE);
	PlayerFixture warrior = makeLevelOne(9782, PlayerClass::WARRIOR, 502, 500, 10);
	equipSword(*warrior.player, 9783);
	Ref<gameobjects::Npc> dummy = makeNpc(DUMMY_NPC_ID);
	Ref<ProbeCalcObserver> shield = observe(*dummy);
	shield->shield = true;
	const auto effectResult = privateMember(EffectResultTag{});

	Ref<Effect> effect = effectOf(*mage.player, *dummy, magicalSkill);
	effectResult(*effect, *dummy, 80, AttackStatus::NORMALHIT, HitType::MAHIT, true, 3, false);
	EXPECT_EQ(shield->shieldChecks, 0);
	EXPECT_EQ(effect->getReflectedDamage(), 0);
	EXPECT_EQ(effect->getShieldDefense(), 0);
	EXPECT_EQ(reservedDamage(*effect, 3), 80);
	EXPECT_FALSE(effect->getReserveds(3)->isSend());
	EXPECT_TRUE(effect->isLaunchSubEffect());

	// through calculateSkillResult: the strike of the physical golden vectors (57), at the template's position 2
	const SkillAttackProbe strike(SkillElement::NONE, 2);
	Ref<Effect> ignored = effectOf(*warrior.player, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*ignored, 27, &strike, true);
	EXPECT_EQ(shield->shieldChecks, 0);
	EXPECT_EQ(reservedDamage(*ignored, 2), 57);
	Ref<Effect> shielded = effectOf(*warrior.player, *dummy, physicalSkill);
	AttackUtil::calculateSkillResult(*shielded, 27, &strike, false);
	EXPECT_EQ(shield->shieldChecks, 1);
	EXPECT_EQ(shield->seenDamage, 57);
	EXPECT_EQ(reservedDamage(*shielded, 2), 50);
}

} // namespace
} // namespace aion::gameserver::model::stats::test
