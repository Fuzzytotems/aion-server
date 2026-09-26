// P5-03, M5b-2 stage 1 work item F-01 (m5b2-plan.md §5): the 26 bodies of EffectTemplate (EffectTemplate.java:227-573), the root every effect
// class of the milestone derives from.
//
// Every case builds a real Effect (its constructors are ported, skillengine/model/Effect.cpp:43-72) and runs EffectTemplate's own body against it
// through ProbeEffect, a subclass that overrides nothing but the abstract applyEffect, so no leaf class (RootEffect, DamageEffect, ...) can answer
// instead. The protected JAXB fields and hooks are made public by using-declarations; the nine private helpers are reached through the
// explicit-instantiation access rule (PrivateAccess below), so the frozen hub header needs no test friend.
//
// **What these cases can reach.** In part 1 of the stage 83 of Effect's 88 bodies were AION_UNPORTED, and nine cases here asserted which unported
// Effect or EffectController body a branch stopped at. Part 2 (the effect-core lane, P5-02b) ported them, and those cases now assert what the
// bodies do: the passive, immunity, forced and dodge arms of calculate by their results; the sub effect calculateSubEffect builds; the sub
// effect startSubEffect applies; the reserved damage of calculateHate's DAMAGE arm; the pre-effect positions; the normal shield of
// isProtectedByShield; and the effect power of checkEffectResistRate at its exact threshold. The draw-order cases (review of part 1) pin the
// order of the random draws the bodies keep: the dodge-or-resist roll before the magical resist rate is computed (checkDodgeOrResistRate), and
// the effect-resist roll before the dodge-or-resist roll, which a failed resist roll skips (isDodgedOrResisted).
//
// Golden values come from the Java expressions, and the real-data rows from skill_templates.xml: 1328 Root (hopb 1239), 10506 [Common]
// Flamethrower's confuse (critprobmod1 100, hopa 60, hopb 60) and 324 Shredding Blow (critprobmod2 10, critadddmg2 50, hoptype SKILLLV without
// hopa/hopb, i.e. the max(1, hate) clamp). Random draws run on a seeded Rnd; each case derives its thresholds from the seed's own reference stream.

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/condition/Condition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/effect/SubEffect.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::skillengine::effect::test {
namespace {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;
using model::HopType;
using runtime::Ptr;
using runtime::Ref;
namespace Rnd = commons::utils::Rnd;

// ---- access ---------------------------------------------------------------------------------------------------------------------------------

/**
 * The private helpers of EffectTemplate (and the protected lists of Conditions and ActionModifiers) are reached through the standard's
 * explicit-instantiation rule: the names in an explicit instantiation are not access-checked ([temp.spec.general]/6), so instantiating
 * PrivateAccess with `&EffectTemplate::isAlteredState` defines the friend `privateMember(Tag)` that hands the member pointer out. No header changes,
 * and a renamed or re-signed helper stops compiling here instead of being silently skipped.
 */
template <class Tag, typename Tag::Type Member>
struct PrivateAccess {
	friend typename Tag::Type privateMember(Tag) { return Member; }
};

struct IsAlteredStateTag {
	using Type = bool (EffectTemplate::*)(StatEnum) const;
	friend Type privateMember(IsAlteredStateTag);
};
template struct PrivateAccess<IsAlteredStateTag, &EffectTemplate::isAlteredState>;

struct GetPenetrationStatTag {
	using Type = std::optional<StatEnum> (EffectTemplate::*)(StatEnum) const;
	friend Type privateMember(GetPenetrationStatTag);
};
template struct PrivateAccess<GetPenetrationStatTag, &EffectTemplate::getPenetrationStat>;

struct IsProtectedByShieldTag {
	using Type = bool (EffectTemplate::*)(Creature&, StatEnum) const;
	friend Type privateMember(IsProtectedByShieldTag);
};
template struct PrivateAccess<IsProtectedByShieldTag, &EffectTemplate::isProtectedByShield>;

struct ValidatePreEffectsTag {
	using Type = bool (EffectTemplate::*)(model::Effect&) const;
	friend Type privateMember(ValidatePreEffectsTag);
};
template struct PrivateAccess<ValidatePreEffectsTag, &EffectTemplate::validatePreEffects>;

struct ValidateEffectConditionsTag {
	using Type = bool (EffectTemplate::*)(model::Effect&) const;
	friend Type privateMember(ValidateEffectConditionsTag);
};
template struct PrivateAccess<ValidateEffectConditionsTag, &EffectTemplate::validateEffectConditions>;

struct EffectSubConditionsCheckTag {
	using Type = bool (EffectTemplate::*)(model::Effect&) const;
	friend Type privateMember(EffectSubConditionsCheckTag);
};
template struct PrivateAccess<EffectSubConditionsCheckTag, &EffectTemplate::effectSubConditionsCheck>;

struct CheckDodgeOrResistRateTag {
	using Type = bool (EffectTemplate::*)(model::Effect&) const;
	friend Type privateMember(CheckDodgeOrResistRateTag);
};
template struct PrivateAccess<CheckDodgeOrResistRateTag, &EffectTemplate::checkDodgeOrResistRate>;

struct AddSuccessEffectTag {
	using Type = void (EffectTemplate::*)(model::Effect&, std::optional<model::SpellStatus>) const;
	friend Type privateMember(AddSuccessEffectTag);
};
template struct PrivateAccess<AddSuccessEffectTag, &EffectTemplate::addSuccessEffect>;

struct IsImmuneToAbnormalTag {
	using Type = bool (EffectTemplate::*)(model::Effect&, StatEnum) const;
	friend Type privateMember(IsImmuneToAbnormalTag);
};
template struct PrivateAccess<IsImmuneToAbnormalTag, &EffectTemplate::isImmuneToAbnormal>;

struct ConditionListTag {
	using Type = std::vector<std::unique_ptr<condition::Condition>> condition::Conditions::*;
	friend Type privateMember(ConditionListTag);
};
template struct PrivateAccess<ConditionListTag, &condition::Conditions::conditions>;

struct ModifierListTag {
	using Type = std::vector<std::unique_ptr<modifier::ActionModifier>> modifier::ActionModifiers::*;
	friend Type privateMember(ModifierListTag);
};
template struct PrivateAccess<ModifierListTag, &modifier::ActionModifiers::actionModifiers>;

// ---- test doubles ---------------------------------------------------------------------------------------------------------------------------

/** EffectTemplate with nothing overridden but the abstract applyEffect, its protected fields and hooks public for the cases */
class ProbeEffect : public EffectTemplate {
public:
	std::string_view javaClassName() const override { return "ProbeEffect"; }

	void applyEffect(model::Effect& /*effect*/) const override { ADD_FAILURE() << "no EffectTemplate body calls the template's applyEffect"; }

	using EffectTemplate::accMod1;
	using EffectTemplate::accMod2;
	using EffectTemplate::critAddDmg1;
	using EffectTemplate::critAddDmg2;
	using EffectTemplate::critProbMod1;
	using EffectTemplate::critProbMod2;
	using EffectTemplate::delta;
	using EffectTemplate::effectConditions;
	using EffectTemplate::element;
	using EffectTemplate::effectSubConditions;
	using EffectTemplate::hopA;
	using EffectTemplate::hopB;
	using EffectTemplate::hopType;
	using EffectTemplate::modifiers;
	using EffectTemplate::noResist;
	using EffectTemplate::position;
	using EffectTemplate::preEffectProb;
	using EffectTemplate::preEffects;
	using EffectTemplate::subEffect;
	using EffectTemplate::value;

	using EffectTemplate::calculateBaseValue;
	using EffectTemplate::isDodgedOrResisted;
	using EffectTemplate::resolveMagicalCritical;
};

/** Java SkillAttackInstantEffect.isNoResist (cannotmiss): a subclass answering the virtual getter instead of the noresist field */
class CannotMissProbeEffect final : public ProbeEffect {
public:
	bool isNoResist() const override { return true; }
};

/** A condition answering validate(Effect) with a fixed value and counting the calls */
class ProbeCondition final : public condition::Condition {
public:
	explicit ProbeCondition(bool answerValue) : answer(answerValue) {}

	std::string_view javaClassName() const override { return "ProbeCondition"; }

	using Condition::validate;

	bool validate(model::Skill& /*env*/) const override {
		ADD_FAILURE() << "EffectTemplate validates its conditions against the Effect, never the Skill";
		return false;
	}

	bool validate(model::Effect& /*effect*/) const override {
		++calls;
		return answer;
	}

	const bool answer;
	mutable int calls = 0;
};

/** A modifier answering check(Effect) with a fixed value and counting the calls */
class ProbeModifier final : public modifier::ActionModifier {
public:
	explicit ProbeModifier(bool passesValue) : passes(passesValue) {}

	std::string_view javaClassName() const override { return "ProbeModifier"; }

	int32_t analyze(model::Effect& /*effect*/) const override {
		ADD_FAILURE() << "EffectTemplate only checks modifiers; DamageEffect and the heals analyze them";
		return 0;
	}

	bool check(model::Effect& /*effect*/) const override {
		++checks;
		return passes;
	}

	const bool passes;
	mutable int checks = 0;
};

/** A Conditions holding one ProbeCondition per answer, in order; `probes` borrows them from the list */
struct ConditionProbes {
	std::unique_ptr<condition::Conditions> conditions = std::make_unique<condition::Conditions>();
	std::vector<const ProbeCondition*> probes;
};

ConditionProbes conditionProbes(std::initializer_list<bool> answers) {
	ConditionProbes result;
	for (bool answer : answers) {
		auto probe = std::make_unique<ProbeCondition>(answer);
		result.probes.push_back(probe.get());
		((*result.conditions).*privateMember(ConditionListTag{})).push_back(std::move(probe));
	}
	return result;
}

/** An ActionModifiers holding one ProbeModifier per answer, in order */
struct ModifierProbes {
	std::unique_ptr<modifier::ActionModifiers> modifiers = std::make_unique<modifier::ActionModifiers>();
	std::vector<const ProbeModifier*> probes;
};

ModifierProbes modifierProbes(std::initializer_list<bool> answers) {
	ModifierProbes result;
	for (bool answer : answers) {
		auto probe = std::make_unique<ProbeModifier>(answer);
		result.probes.push_back(probe.get());
		((*result.modifiers).*privateMember(ModifierListTag{})).push_back(std::move(probe));
	}
	return result;
}

std::unique_ptr<SubEffect> subEffectOf(int32_t skillId, int32_t chance, bool addEffect = false) {
	auto sub = std::make_unique<SubEffect>();
	sub->skillId = skillId;
	sub->chance = chance;
	sub->addEffect = addEffect;
	return sub;
}

/** The first `count` values Rnd::chance() gives after seeding the calling thread with `seed` (leaves the thread seeded and advanced) */
std::vector<float> chanceStream(uint64_t seed, int count) {
	Rnd::seedCurrentThreadForTests(seed);
	std::vector<float> stream;
	for (int i = 0; i < count; ++i)
		stream.push_back(Rnd::chance());
	return stream;
}

inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

/** Captures the messages of one logger ("level|message" per line) while it exists (the pattern of tests/ai/AiTestSupport.h) */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	/** the captured lines, "\n"-terminated (spdlog ends them with the platform's "\r\n" on Windows) */
	std::string text() const {
		std::string captured = stream.str();
		std::erase(captured, '\r');
		return captured;
	}

private:
	std::string name;
	std::ostringstream stream;
};

// ---- fixture --------------------------------------------------------------------------------------------------------------------------------

constexpr uint64_t SEED = 20260923;

/** An active magical debuff with one effect position, the shape of 1328 Root without its data */
const char* const ACTIVE_SKILL_XML = R"(<skill_template skill_id="9001" name="probe" nameId="1" stack="PROBE" lvl="1" skilltype="MAGICAL")"
									 R"( skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" duration="0"/>)";

/** A passive skill (Java SkillTemplate.isPassive: activation PASSIVE) */
const char* const PASSIVE_SKILL_XML = R"(<skill_template skill_id="9002" name="probe passive" nameId="1" stack="PROBE_P" lvl="1")"
									  R"( skilltype="PHYSICAL" skillsubtype="BUFF" activation="PASSIVE" duration="0"/>)";

class EffectTemplateTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 23));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		savedRnd.emplace(Rnd::generator());
	}

	void TearDown() override {
		Rnd::generator() = *savedRnd;
		if (skillDataPublished)
			dataholders::DataManager::SKILL_DATA.resetForTests();
		spawnGroups.clear();
		if (npcSkillDataPublished)
			dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	struct PlayerFixture {
		Ref<gameserver::model::account::Account> account;
		Ref<gameserver::model::gameobjects::player::PlayerCommonData> commonData;
		Ref<Player> player;
	};

	/** Java PlayerService.getPlayer: account, common data, appearance, account data and warehouse, the Player, its known list and effect controller */
	static PlayerFixture makePlayer(int32_t objectId) {
		namespace m = gameserver::model;
		PlayerFixture f;
		f.account = m::account::Account::create(7000 + objectId);
		f.commonData = m::gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Tmpl" + std::to_string(objectId));
		f.commonData->setRace(m::Race::ELYOS);
		f.commonData->setPlayerClass(m::PlayerClass::MAGE);
		Ref<m::gameobjects::player::PlayerAppearance> appearance = m::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<m::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<m::items::storage::PlayerStorage>(*f.account, m::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = m::gameobjects::VisibleObject::create<Player>(*f.account->getPlayerAccountData(objectId), *f.account);
		f.player->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		f.player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*f.player));
		f.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*f.player));
		return f;
	}

	/** Binds a <skill_template> and keeps it for the test (effect templates are immortal static data in the server) */
	const model::SkillTemplate* bindSkill(const std::string& xmlText) {
		xml::LoadContext context;
		skills.push_back(xml::bindString<model::SkillTemplate>(context, xmlText));
		return skills.back().get();
	}

	/** The effect template at `index` of a bound skill's <effects> */
	static const EffectTemplate& effectOf(const model::SkillTemplate& skill, size_t index) {
		return *skill.getEffects()->getEffects().at(index);
	}

	/** Publishes DataManager::SKILL_DATA for the sub effect lookups of calculateSubEffect; TearDown forgets it again */
	void publishSkillData(const std::string& templatesXml) {
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(skillDataContext, "<skill_data>" + templatesXml + "</skill_data>"));
		skillDataPublished = true;
	}

	/** One application of `skill` at `level` from `effector` to `effected` (Java `new Effect(effector, effected, skillTemplate, skillLevel)`) */
	static Ref<model::Effect> effectFor(Creature& effector, Creature& effected, const model::SkillTemplate* skill, int32_t level) {
		return model::Effect::create(effector, effected, skill, level);
	}

	/**
	 * An npc whose template has no run speed - the npcs isImmuneToAbnormal makes immune to PULLED, STAGGER and STUMBLE (EffectTemplate.java:536-538)
	 * - with the known list and effect controller its spawn gives it. Npc's constructor reads NPC_SKILL_DATA (empty here).
	 */
	Ref<gameserver::model::gameobjects::Npc> makeRootedNpc(int32_t npcId) {
		namespace m = gameserver::model;
		if (!dataholders::DataManager::NPC_SKILL_DATA) {
			dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
			npcSkillDataPublished = true;
		}
		xml::LoadContext context;
		const m::templates::npc::NpcTemplate* npcTemplate = xml::bindString<m::templates::npc::NpcTemplate>(context,
			R"(<npc_template name_id="1" npc_id=")" + std::to_string(npcId) + R"(" level="4" name="rooted" attack_speed="2000" arange="2")"
				R"( rating="NORMAL" tribe="GENERAL"><stats maxHp="2522" maxMp="100" attack="16"><speeds walk="0" run="0"/></stats></npc_template>)")
			.release(); // immortal static data, like the holder keeps it
		spawnGroups.push_back(m::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr));
		m::templates::spawns::SpawnTemplate& spawnTemplate = spawnGroups.back()->addSpawnTemplate(std::make_unique<TemplateTestSpawn>(*spawnGroups.back()));
		Ref<m::gameobjects::Npc> npc =
			m::gameobjects::VisibleObject::create<m::gameobjects::Npc>(std::make_unique<controllers::NpcController>(), spawnTemplate, npcTemplate);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		return npc;
	}

	/** A spawn template the npc of makeRootedNpc returns to */
	class TemplateTestSpawn final : public gameserver::model::templates::spawns::SpawnTemplate {
	public:
		explicit TemplateTestSpawn(gameserver::model::templates::spawns::SpawnGroup& group)
			: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
	};

	runtime::ManualClock clock{0};
	std::optional<Rnd::Xoshiro256PlusPlus> savedRnd;
	std::vector<std::unique_ptr<model::SkillTemplate>> skills;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnGroup>> spawnGroups;
	xml::LoadContext skillDataContext;
	bool skillDataPublished = false;
	bool npcSkillDataPublished = false;
};

// ---- the template arithmetic ------------------------------------------------------------------------------------------------------------------

/** Java calculateBaseValue (EffectTemplate.java:264-266): value + delta * skillLevel - the value a skill description states */
TEST_F(EffectTemplateTest, BaseValueIsValuePlusDeltaTimesTheSkillLevel) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1101);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	ProbeEffect probe;
	probe.value = 141;
	probe.delta = 7;

	EXPECT_EQ(probe.calculateBaseValue(*effectFor(*caster.player, *caster.player, skill, 1)), 148) << "141 + 7 * 1";
	EXPECT_EQ(probe.calculateBaseValue(*effectFor(*caster.player, *caster.player, skill, 3)), 162) << "141 + 7 * 3";
	EXPECT_EQ(probe.calculateBaseValue(*effectFor(*caster.player, *caster.player, skill, 10)), 211) << "141 + 7 * 10";

	probe.delta = 0; // 1282 Flame Bolt: <spellatkinstant value="141" .../> has no delta, so every level states 141
	EXPECT_EQ(probe.calculateBaseValue(*effectFor(*caster.player, *caster.player, skill, 7)), 141);

	probe.value = -20;
	probe.delta = -3;
	EXPECT_EQ(probe.calculateBaseValue(*effectFor(*caster.player, *caster.player, skill, 4)), -32) << "no clamp: -20 + -3 * 4";
}

/** Java calculateCritAddDmg (:268-270) and calculateCritProbMod (:272-274): <field>2 + <field>1 * skillLevel, critprobmod2 defaulting to 100 */
TEST_F(EffectTemplateTest, CriticalModifiersAreTheSecondFieldPlusTheFirstTimesTheSkillLevel) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1201);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	Ref<model::Effect> level1 = effectFor(*caster.player, *caster.player, skill, 1);
	Ref<model::Effect> level4 = effectFor(*caster.player, *caster.player, skill, 4);

	ProbeEffect probe;
	EXPECT_EQ(probe.calculateCritProbMod(*level4), 100) << "Java field initializers: critprobmod2 = 100, critprobmod1 = 0";
	EXPECT_EQ(probe.calculateCritAddDmg(*level4), 0) << "critadddmg1 = critadddmg2 = 0";

	probe.critProbMod2 = 30;
	probe.critProbMod1 = 5;
	probe.critAddDmg2 = 50;
	probe.critAddDmg1 = 3;
	EXPECT_EQ(probe.calculateCritProbMod(*level1), 35) << "30 + 5 * 1";
	EXPECT_EQ(probe.calculateCritProbMod(*level4), 50) << "30 + 5 * 4";
	EXPECT_EQ(probe.calculateCritAddDmg(*level1), 53) << "50 + 3 * 1";
	EXPECT_EQ(probe.calculateCritAddDmg(*level4), 62) << "50 + 3 * 4";
}

/** Java calculateHate (:443-460): SKILLLV is hopb + hopa * skillLevel, at least 1; no hoptype is no hate at all */
TEST_F(EffectTemplateTest, SkillLevelHateIsHopbPlusHopaTimesTheLevelAndAtLeastOne) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1301);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	Ref<model::Effect> level1 = effectFor(*caster.player, *caster.player, skill, 1);
	Ref<model::Effect> level5 = effectFor(*caster.player, *caster.player, skill, 5);

	ProbeEffect probe;
	probe.hopA = 60;
	probe.hopB = 40;
	EXPECT_EQ(probe.calculateHate(*level5), 0) << "no hoptype: `return 0`, not the clamp and not the formula";

	probe.hopType = HopType::SKILLLV;
	EXPECT_EQ(probe.calculateHate(*level1), 100) << "40 + 60 * 1";
	EXPECT_EQ(probe.calculateHate(*level5), 340) << "40 + 60 * 5";

	probe.hopA = 0;
	probe.hopB = 0;
	EXPECT_EQ(probe.calculateHate(*level5), 1) << "Math.max(1, 0)";
	probe.hopB = -500;
	probe.hopA = 20;
	EXPECT_EQ(probe.calculateHate(*level5), 1) << "Math.max(1, -500 + 20 * 5)";
	probe.hopB = -99;
	EXPECT_EQ(probe.calculateHate(*level5), 1) << "Math.max(1, 1)";
	probe.hopB = -98;
	EXPECT_EQ(probe.calculateHate(*level5), 2) << "Math.max(1, 2): the clamp is at 1, not at 0";
}

/**
 * The DAMAGE arm reads the reserved damage of position 0 (Effect.getReserveds, an empty value 0 when there is none) and falls through into the
 * SKILLLV sum (:448-452): reserved + hopb + hopa * skillLevel, at least 1. An out-of-range HopType takes the default arm, whose exception is Java's
 * UnsupportedOperationException.
 */
TEST_F(EffectTemplateTest, DamageHateReadsTheReservedDamageAndAnUnknownHopTypeThrows) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1401);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, skill, 2);

	ProbeEffect probe;
	probe.hopType = HopType::DAMAGE;
	probe.hopB = 7;
	EXPECT_EQ(probe.calculateHate(*effect), 7) << "no reserved value at position 0: 0 + 7 + 0 * 2";
	effect->setReserveds(*model::EffectReserved::create(1, 900, model::EffectReserved::ResourceType::HP, true), true);
	EXPECT_EQ(probe.calculateHate(*effect), 7) << "position 1 is not the position the DAMAGE arm reads";
	effect->setReserveds(*model::EffectReserved::create(0, 250, model::EffectReserved::ResourceType::HP, true), true);
	EXPECT_EQ(probe.calculateHate(*effect), 257) << "250 + 7: the reserved damage of position 0";
	probe.hopA = 3;
	EXPECT_EQ(probe.calculateHate(*effect), 263) << "the fall-through adds hopa * skillLevel: 250 + 7 + 3 * 2";
	probe.hopA = 0;
	probe.hopB = -400;
	EXPECT_EQ(probe.calculateHate(*effect), 1) << "Math.max(1, 250 - 400)";

	probe.hopType = static_cast<HopType>(7); // no such constant: HopType has DAMAGE and SKILLLV only
	try {
		probe.calculateHate(*effect);
		FAIL() << "the default arm must throw";
	} catch (const runtime::UnportedException& unported) {
		FAIL() << "the default arm is Java's UnsupportedOperationException, not a stub: " << unported.what();
	} catch (const commons::utils::UnsupportedOperationException& unsupported) {
		EXPECT_NE(std::string(unsupported.what()).find(" for hate calculation"), std::string::npos) << unsupported.what();
	}
}

/**
 * The same arithmetic on templates bound from skill_templates.xml, through the real subclasses: calculateHate and the critical modifiers are
 * non-virtual, so a RootEffect or a ConfuseEffect answers EffectTemplate's own body from its own bound attributes.
 */
TEST_F(EffectTemplateTest, RealTemplatesAnswerTheirBoundHateAndCriticalModifiers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1501);

	// skill_templates.xml:19302-19311, the level-1 Mage's Root
	const model::SkillTemplate* root = bindSkill(
		R"(<skill_template skill_id="1328" name="Root" nameId="2287465" cooldownId="277" group="MA_ROOT" stack="MA_ROOT" lvl="1" skilltype="MAGICAL")"
		R"( skill_category="PHYSICAL_DEBUFF" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="ALL" req_dispel_level="1" req_dispel_count="10")"
		R"( activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
		R"( apply_magical_critical="true" apply_casting_time_bonus="true">)"
		R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE"/>)"
		R"(<endconditions><mp value="38" delta="0"/></endconditions>)"
		R"(<effects><root resistchance="10" duration2="20000" effectid="20003" e="1" accmod2="500" element="WATER" hoptype="SKILLLV")"
		R"( hopb="1239"/></effects><motion name="debuff" speed="50" instant_skill="true"/></skill_template>)");
	const EffectTemplate& rootEffect = effectOf(*root, 0);
	ASSERT_EQ(rootEffect.javaClassName(), "RootEffect");
	EXPECT_EQ(rootEffect.calculateHate(*effectFor(*caster.player, *caster.player, root, 1)), 1239) << "hopb 1239 + hopa 0 * 1";
	EXPECT_EQ(rootEffect.calculateCritProbMod(*effectFor(*caster.player, *caster.player, root, 1)), 100) << "no critprobmod: 100 + 0";

	// skill_templates.xml:102084-102094, [Common] Flamethrower's third position
	const model::SkillTemplate* flamethrower = bindSkill(
		R"(<skill_template skill_id="10506" name="[Common] Flamethrower" nameId="2281102" stack="ITEM_IDEVENT01_FIRETHROW" lvl="1")"
		R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0" hostile_type="DIRECT">)"
		R"(<properties first_target="ME" first_target_range="2" target_relation="ENEMY" target_type="AREA" target_maxcount="20")"
		R"( effective_altitude="30" effective_angle="90" effective_dist="20"/>)"
		R"(<useconditions><move_casting allow="false"/></useconditions><effects>)"
		R"(<spellatkinstant value="1500" e="1" noresist="true" element="FIRE" hoptype="DAMAGE"/>)"
		R"(<spellatk checktime="1000" value="800" duration2="5000" e="2" noresist="true" element="FIRE" preeffect="1" hoptype="DAMAGE"/>)"
		R"(<confuse duration2="5000" e="3" noresist="true" element="FIRE" preeffect="2" critprobmod1="100" hoptype="SKILLLV" hopb="60")"
		R"( hopa="60"/></effects><motion name="pointfire"/></skill_template>)");
	const EffectTemplate& confuse = effectOf(*flamethrower, 2);
	ASSERT_EQ(confuse.javaClassName(), "ConfuseEffect");
	Ref<model::Effect> flameLevel3 = effectFor(*caster.player, *caster.player, flamethrower, 3);
	EXPECT_EQ(confuse.calculateHate(*flameLevel3), 240) << "60 + 60 * 3";
	EXPECT_EQ(confuse.calculateCritProbMod(*flameLevel3), 400) << "critprobmod2 default 100 + critprobmod1 100 * 3";
	EXPECT_EQ(confuse.calculateCritAddDmg(*flameLevel3), 0);

	// skill_templates.xml:4026-4038, Shredding Blow: hoptype SKILLLV without hopa or hopb, so the hate is the clamp
	const model::SkillTemplate* shredding = bindSkill(
		R"(<skill_template skill_id="324" name="Shredding Blow" nameId="2285603" group="IDSWEEP_SKILL" stack="IDSWEEP_KN1_BASE" lvl="1")"
		R"( skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="5" duration="0" cancel_rate="10")"
		R"( hostile_type="INDIRECT"><properties first_target="ME" first_target_range="1" target_relation="ENEMY" target_type="AREA")"
		R"( target_maxcount="8" effective_altitude="4" effective_range="7"/><startconditions><form value="FORM1"/></startconditions>)"
		R"(<endconditions><mp value="10" delta="0"/></endconditions><effects><noreducespellatk value="600" e="1" basiclvl="2")"
		R"( noresist="true" element="FIRE" critprobmod2="10" critadddmg2="50" hoptype="SKILLLV"/></effects><motion name="sweepkn1"/>)"
		R"(</skill_template>)");
	const EffectTemplate& blow = effectOf(*shredding, 0);
	Ref<model::Effect> blowLevel2 = effectFor(*caster.player, *caster.player, shredding, 2);
	EXPECT_EQ(blow.calculateHate(*blowLevel2), 1) << "Math.max(1, 0 + 0 * 2)";
	EXPECT_EQ(blow.calculateCritProbMod(*blowLevel2), 10) << "critprobmod2 10 + 0";
	EXPECT_EQ(blow.calculateCritAddDmg(*blowLevel2), 50) << "critadddmg2 50 + 0";
}

// ---- modifiers and sub effects ------------------------------------------------------------------------------------------------------------------

/** Java getActionModifiers (:227-238): null without <modifiers>, else the FIRST modifier whose check passes, checking none after it */
TEST_F(EffectTemplateTest, ActionModifiersAnswerTheFirstModifierWhoseCheckPasses) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1601);
	Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, bindSkill(ACTIVE_SKILL_XML), 1);

	ProbeEffect probe;
	EXPECT_EQ(probe.getActionModifiers(*effect), nullptr) << "no <modifiers>";

	ModifierProbes none = modifierProbes({});
	probe.modifiers = std::move(none.modifiers);
	EXPECT_EQ(probe.getActionModifiers(*effect), nullptr) << "an empty <modifiers/>";

	ModifierProbes list = modifierProbes({false, true, true});
	probe.modifiers = std::move(list.modifiers);
	EXPECT_EQ(probe.getActionModifiers(*effect), list.probes[1]) << "the first passing modifier, not the last";
	EXPECT_EQ(list.probes[0]->checks, 1);
	EXPECT_EQ(list.probes[1]->checks, 1);
	EXPECT_EQ(list.probes[2]->checks, 0) << "Only one of modifiers will be applied now: the loop returns on the first match";

	ModifierProbes failing = modifierProbes({false, false});
	probe.modifiers = std::move(failing.modifiers);
	EXPECT_EQ(probe.getActionModifiers(*effect), nullptr);
	EXPECT_EQ(failing.probes[0]->checks, 1);
	EXPECT_EQ(failing.probes[1]->checks, 1);
}

/**
 * Java calculateSubEffect (:398-434), its four exits before the sub effect is built: no <subeffect>; <modifiers> present but none passing (the
 * sub conditions are not even asked); sub conditions failing (the effect is marked aborted and no chance is rolled); the chance roll failing.
 */
TEST_F(EffectTemplateTest, SubEffectStopsAtTheModifierTheSubConditionAndTheChanceGates) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1701);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	const std::vector<float> stream = chanceStream(SEED, 2);

	{ // no <subeffect>: nothing is asked, marked or rolled, even with failing sub conditions
		Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, skill, 1);
		ProbeEffect probe;
		ConditionProbes sub = conditionProbes({false});
		probe.effectSubConditions = std::move(sub.conditions);
		Rnd::seedCurrentThreadForTests(SEED);
		probe.calculateSubEffect(*effect);
		EXPECT_FALSE(effect->isSubEffectAbortedBySubConditions());
		EXPECT_EQ(sub.probes[0]->calls, 0);
		EXPECT_EQ(Rnd::chance(), stream[0]) << "no chance was rolled";
	}
	{ // <modifiers> with no passing modifier: return before the sub conditions
		Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, skill, 1);
		ProbeEffect probe;
		probe.subEffect = subEffectOf(8217, 100);
		ModifierProbes mods = modifierProbes({false});
		probe.modifiers = std::move(mods.modifiers);
		ConditionProbes sub = conditionProbes({false});
		probe.effectSubConditions = std::move(sub.conditions);
		probe.calculateSubEffect(*effect);
		EXPECT_EQ(mods.probes[0]->checks, 1);
		EXPECT_EQ(sub.probes[0]->calls, 0) << "the modifier gate comes first";
		EXPECT_FALSE(effect->isSubEffectAbortedBySubConditions());
	}
	{ // a passing modifier opens the gate; failing sub conditions mark the effect and roll nothing
		Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, skill, 1);
		ProbeEffect probe;
		probe.subEffect = subEffectOf(8217, 100);
		ModifierProbes mods = modifierProbes({true});
		probe.modifiers = std::move(mods.modifiers);
		ConditionProbes sub = conditionProbes({true, false});
		probe.effectSubConditions = std::move(sub.conditions);
		Rnd::seedCurrentThreadForTests(SEED);
		probe.calculateSubEffect(*effect);
		EXPECT_EQ(sub.probes[0]->calls, 1);
		EXPECT_EQ(sub.probes[1]->calls, 1);
		EXPECT_TRUE(effect->isSubEffectAbortedBySubConditions()) << "setSubEffectAborted(true)";
		EXPECT_EQ(Rnd::chance(), stream[0]) << "an aborted sub effect rolls no chance";
	}
	{ // the chance roll: Rnd.chance() >= chance fails. The seed's first roll decides both thresholds
		ASSERT_GT(stream[0], 1.0f);
		ASSERT_LT(stream[0], 99.0f);
		const int32_t atOrBelowRoll = static_cast<int32_t>(stream[0]); // roll >= threshold: no sub effect
		Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, skill, 1);
		ProbeEffect probe;
		probe.subEffect = subEffectOf(8217, atOrBelowRoll);
		ConditionProbes sub = conditionProbes({true});
		probe.effectSubConditions = std::move(sub.conditions);
		Rnd::seedCurrentThreadForTests(SEED);
		EXPECT_NO_THROW(probe.calculateSubEffect(*effect)) << "roll " << stream[0] << " >= chance " << atOrBelowRoll;
		EXPECT_FALSE(effect->isSubEffectAbortedBySubConditions()) << "passing sub conditions do not mark the effect";
		EXPECT_EQ(effect->getSubEffect(), nullptr);
		EXPECT_EQ(Rnd::chance(), stream[1]) << "exactly one chance was rolled";

		probe.subEffect = subEffectOf(8217, 0);
		Rnd::seedCurrentThreadForTests(SEED);
		EXPECT_NO_THROW(probe.calculateSubEffect(*effect)) << "chance 0 never triggers";
	}
}

/**
 * Past the chance gate calculateSubEffect looks the sub skill up in SKILL_DATA and builds `new Effect(effector, originalEffected, template, level,
 * null, forceType, true, null)` (:420-433): the sub effect copies the effect's shield defense, gets the accuracy boost, is initialized and becomes
 * the effect's sub effect, whose spell status, sub effect type and target location the effect takes over unless it dodged or resisted. A plain
 * <subeffect> is level 1 with the effect's accuracy boost; an addeffect one (signet bursts) takes the signet bursted count as its level and
 * Short.MAX_VALUE as its boost. A missing template is Java's NullPointerException (the Effect constructor dereferences it).
 */
TEST_F(EffectTemplateTest, APassingChanceBuildsTheSubEffectFromSkillData) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1801);
	PlayerFixture target = makePlayer(1802);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	publishSkillData(R"(<skill_template skill_id="8217" name="stun proc" nameId="1" stack="PROC_STUN" lvl="1" skilltype="PHYSICAL")"
					 R"( skillsubtype="DEBUFF" activation="PROVOKED" duration="0"/>)");
	const std::vector<float> stream = chanceStream(SEED, 1);
	ASSERT_LT(stream[0], 99.0f);

	Ref<model::Effect> effect = effectFor(*caster.player, *target.player, skill, 1);
	effect->setShieldDefense(2);
	effect->setAccModBoost(40);
	effect->setSpellStatus(model::SpellStatus::PARRY);
	ProbeEffect probe;
	probe.subEffect = subEffectOf(8217, static_cast<int32_t>(stream[0]) + 1); // roll < chance: triggers
	Rnd::seedCurrentThreadForTests(SEED);
	probe.calculateSubEffect(*effect);
	Ptr<model::Effect> built = effect->getSubEffect();
	ASSERT_TRUE(built) << "the lookup, the construction and initialize ran";
	EXPECT_EQ(built->getSkillId(), 8217);
	EXPECT_TRUE(built->isSubEffect());
	EXPECT_EQ(built->getSkillLevel(), 1);
	EXPECT_EQ(built->getEffector().get(), caster.player.get());
	EXPECT_EQ(built->getOriginalEffected().get(), target.player.get());
	EXPECT_EQ(built->getShieldDefense(), 2) << "newEffect.setShieldDefense(effect.getShieldDefense())";
	EXPECT_EQ(built->getAccModBoost(), 40) << "a plain sub effect keeps the effect's accuracy boost";
	EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::NONE) << "the sub effect's spell status (it neither dodged nor resisted)";

	effect->setSignetBurstedCount(3);
	probe.subEffect = subEffectOf(8217, 100, true); // addeffect: level = signet bursted count, accBoost = Short.MAX_VALUE
	probe.calculateSubEffect(*effect);
	Ptr<model::Effect> burst = effect->getSubEffect();
	ASSERT_TRUE(burst);
	EXPECT_NE(burst.get(), built.get()) << "a new sub effect replaces the first";
	EXPECT_EQ(burst->getSkillLevel(), 3);
	EXPECT_EQ(burst->getAccModBoost(), std::numeric_limits<int16_t>::max());

	effect->setSubEffect(nullptr);
	probe.subEffect = subEffectOf(4242, 100); // not in SKILL_DATA
	EXPECT_THROW(probe.calculateSubEffect(*effect), runtime::NullPointerException);
	EXPECT_EQ(effect->getSubEffect(), nullptr);
}

/** A template whose applyEffect counts the calls (the sub effect's template in StartSubEffect...) */
class ApplyCountingProbe final : public EffectTemplate {
public:
	std::string_view javaClassName() const override { return "ApplyCountingProbe"; }
	void applyEffect(model::Effect& /*effect*/) const override { ++applied; }
	using EffectTemplate::position;
	mutable int applied = 0;
};

/**
 * Java startSubEffect (:462-470): no <subeffect>, an aborted one or no built sub effect apply nothing; otherwise the sub effect's applyEffect
 * (Effect.applyEffect), which applies its success effects. The built sub effect is a passive one, because Effect.shouldApplyFurtherEffects
 * applies nothing to the fixture's unspawned player otherwise (Effect.java:640-641).
 */
TEST_F(EffectTemplateTest, StartSubEffectAppliesOnlyABuiltSubEffectThatWasNotAborted) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(1901);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, skill, 1);
	ApplyCountingProbe subTemplate;
	subTemplate.position = 1;
	Ref<model::Effect> built = effectFor(*caster.player, *caster.player, bindSkill(PASSIVE_SKILL_XML), 1);
	built->addSuccessEffect(&subTemplate);
	effect->setSubEffect(built);

	ProbeEffect withoutSub;
	withoutSub.startSubEffect(*effect);
	EXPECT_EQ(subTemplate.applied, 0) << "no <subeffect>: the built sub effect is not this template's";

	ProbeEffect probe;
	probe.subEffect = subEffectOf(8217, 100);
	effect->setSubEffectAborted(true);
	probe.startSubEffect(*effect);
	EXPECT_EQ(subTemplate.applied, 0) << "aborted by its sub conditions";

	effect->setSubEffectAborted(false);
	effect->setSubEffect(nullptr);
	EXPECT_NO_THROW(probe.startSubEffect(*effect)) << "nothing was built (the chance failed)";

	effect->setSubEffect(built);
	probe.startSubEffect(*effect);
	EXPECT_EQ(subTemplate.applied, 1) << "the built sub effect was applied";
}

// ---- the calculate chain ------------------------------------------------------------------------------------------------------------------------

/** Java's empty hooks (:328, :381, :477, :485): the root answers nothing and changes nothing */
TEST_F(EffectTemplateTest, TheEmptyHooksDoNothing) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2001);
	Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, bindSkill(ACTIVE_SKILL_XML), 1);
	ProbeEffect probe;
	EXPECT_NO_THROW(probe.startEffect(*effect));
	EXPECT_NO_THROW(probe.onPeriodicAction(*effect));
	EXPECT_NO_THROW(probe.endEffect(*effect));
	EXPECT_NO_THROW(probe.resolveMagicalCritical(*effect));
	EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::NONE);
	EXPECT_EQ(effect->getSubEffect(), nullptr);
}

/**
 * Java calculate (:281-322) arm by arm. A passive skill adds the success effect (and the spell status) before anything else - even for an
 * altered-state stat the effected is immune to. Any other skill is refused by isImmuneToAbnormal for an altered-state stat the effected is immune
 * to: an npc without run speed for PULLED, STAGGER and STUMBLE (:536-538), not for STUN. A forced effect skips the conditions, the pre-effects
 * and the dodge and resist rolls. The one- and three-argument overloads delegate with a null stat and spell status.
 */
TEST_F(EffectTemplateTest, CalculateTakesThePassiveTheImmunityAndTheForcedArmsInJavaOrder) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2101);
	Ref<gameserver::model::gameobjects::Npc> rooted = makeRootedNpc(700211);
	const model::SkillTemplate* passiveSkill = bindSkill(PASSIVE_SKILL_XML);
	const model::SkillTemplate* activeSkill = bindSkill(ACTIVE_SKILL_XML);
	ProbeEffect probe;
	probe.position = 1;

	Ref<model::Effect> passive = effectFor(*caster.player, *rooted, passiveSkill, 1);
	EXPECT_TRUE(probe.calculate(*passive, StatEnum::STAGGER_RESISTANCE, model::SpellStatus::STUMBLE)) << "passive wins over the immunity";
	EXPECT_TRUE(passive->isInSuccessEffects(1));
	EXPECT_EQ(passive->getSpellStatus(), model::SpellStatus::STUMBLE);

	probe.noResist = true; // the dodge and resist rolls are the business of the draw-order cases
	Ref<model::Effect> immune = effectFor(*caster.player, *rooted, activeSkill, 1);
	EXPECT_FALSE(probe.calculate(*immune, StatEnum::STAGGER_RESISTANCE, model::SpellStatus::STUMBLE)) << "a rooted npc is immune to STAGGER";
	EXPECT_FALSE(immune->isInSuccessEffects(1));
	EXPECT_EQ(immune->getSpellStatus(), model::SpellStatus::NONE);
	EXPECT_TRUE(probe.calculate(*immune, StatEnum::STUN_RESISTANCE, model::SpellStatus::STUMBLE)) << "but not to STUN";
	EXPECT_TRUE(immune->isInSuccessEffects(1));
	EXPECT_EQ(immune->getSpellStatus(), model::SpellStatus::STUMBLE);
	Ref<model::Effect> bleeding = effectFor(*caster.player, *rooted, activeSkill, 1);
	EXPECT_TRUE(probe.calculate(*bleeding, StatEnum::BLEED_RESISTANCE, std::nullopt)) << "BLEED is no altered state";

	Ref<model::Effect> viaOneArgument = effectFor(*caster.player, *rooted, activeSkill, 1);
	probe.calculate(*viaOneArgument);
	EXPECT_TRUE(viaOneArgument->isInSuccessEffects(1)) << "calculate(effect) passes a null stat and spell status";
	EXPECT_EQ(viaOneArgument->getSpellStatus(), model::SpellStatus::NONE);

	ConditionProbes failing = conditionProbes({false});
	probe.effectConditions = std::move(failing.conditions);
	probe.noResist = false;
	Ref<model::Effect> conditioned = effectFor(*caster.player, *rooted, activeSkill, 1);
	EXPECT_FALSE(probe.calculate(*conditioned, std::nullopt, std::nullopt)) << "a failing condition refuses the effect";
	EXPECT_EQ(failing.probes[0]->calls, 1);
	Ref<model::Effect> forced = effectFor(*caster.player, *rooted, activeSkill, 1);
	forced->setForceType(model::Effect::ForceType::DEFAULT);
	const std::vector<float> stream = chanceStream(SEED, 1);
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE(probe.calculate(*forced, std::nullopt, std::nullopt)) << "a forced effect skips the conditions and the dodge roll";
	EXPECT_EQ(failing.probes[0]->calls, 1) << "the condition was not asked";
	EXPECT_EQ(Rnd::chance(), stream[0]) << "no roll";
	EXPECT_TRUE(forced->isInSuccessEffects(1));
}

/** An attack-calc observer that draws one Rnd.get(1, 1000) whenever it is asked for RESIST, so a case can see WHEN the resist rate was computed */
class DrawingObserver final : public controllers::observer::AttackCalcObserver {
	AION_MAKE_REF_FRIEND
public:
	static Ref<DrawingObserver> create() { return runtime::makeRef<DrawingObserver>(); }
	bool checkStatus(controllers::attack::AttackStatus status) override {
		if (status == controllers::attack::AttackStatus::RESIST)
			drawn.push_back(Rnd::get(1, 1000));
		return false;
	}
	std::vector<int32_t> drawn;

protected:
	DrawingObserver() = default;
	~DrawingObserver() override = default;
};

/** The values Rnd.get(1, 1000) gives after seeding the calling thread with `seed` */
std::vector<int32_t> rollStream(uint64_t seed, int count) {
	Rnd::seedCurrentThreadForTests(seed);
	std::vector<int32_t> stream;
	for (int i = 0; i < count; ++i)
		stream.push_back(Rnd::get(1, 1000));
	return stream;
}

/** Adds `value` to one stat of the creature through a stat function without an owner (as RoahCustomInstanceHandler does) */
void addStat(Creature& creature, StatEnum stat, int32_t value) {
	creature.getGameStats()->addEffect(nullptr,
		{Ptr<gameserver::model::stats::calc::functions::IStatFunction>(
			gameserver::model::stats::calc::functions::RcStatFunction<gameserver::model::stats::calc::functions::StatAddFunction>::create(stat, value, true))});
}

/**
 * Java isDodgedOrResisted (:347-349): a noresist template - the field or the virtual getter SkillAttackInstantEffect overrides for cannotmiss -
 * is never dodged or resisted and draws nothing. Otherwise a null stat draws only the dodge roll (checkEffectResistRate answers true without
 * one), and a stat draws the effect-resist roll first and then the dodge roll (physical: StatFunctions.checkIsDodgedHit's Rnd.nextInt(1000)).
 */
TEST_F(EffectTemplateTest, NoResistSkipsTheResistAndDodgeChecks) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2201);
	PlayerFixture target = makePlayer(2202);
	Ref<model::Effect> effect = effectFor(*caster.player, *target.player, bindSkill(ACTIVE_SKILL_XML), 1);
	const std::vector<float> stream = chanceStream(SEED, 1);
	auto nextAfter = [](std::initializer_list<int> calls) {
		Rnd::seedCurrentThreadForTests(SEED);
		for (int call : calls)
			static_cast<void>(call == 0 ? Rnd::get(1, 1000) : Rnd::nextInt(1000)); // 0: the resist roll, 1: the physical dodge roll
		return Rnd::chance();
	};

	ProbeEffect probe;
	probe.noResist = true;
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_FALSE(probe.isDodgedOrResisted(*effect, StatEnum::STUN_RESISTANCE));
	EXPECT_FALSE(probe.isDodgedOrResisted(*effect, std::nullopt));
	CannotMissProbeEffect cannotMiss;
	ASSERT_FALSE(cannotMiss.noResist);
	EXPECT_FALSE(cannotMiss.isDodgedOrResisted(*effect, StatEnum::STUN_RESISTANCE)) << "isNoResist() is virtual; the field alone is false";
	EXPECT_EQ(Rnd::chance(), stream[0]) << "noresist draws nothing";

	probe.noResist = false;
	const float afterDodgeRoll = nextAfter({1});
	const float afterResistAndDodgeRolls = nextAfter({0, 1});
	ASSERT_NE(afterDodgeRoll, afterResistAndDodgeRolls);
	Rnd::seedCurrentThreadForTests(SEED);
	probe.isDodgedOrResisted(*effect, std::nullopt);
	EXPECT_EQ(Rnd::chance(), afterDodgeRoll) << "a null stat: only the dodge roll";
	Rnd::seedCurrentThreadForTests(SEED);
	probe.isDodgedOrResisted(*effect, StatEnum::STUN_RESISTANCE);
	EXPECT_EQ(Rnd::chance(), afterResistAndDodgeRolls) << "a stat: the effect-resist roll, then the dodge roll";
}

/**
 * Draw order kept by checkDodgeOrResistRate (EffectTemplate.cpp; EffectTemplate.java:358): Java evaluates `Rnd.get(1, 1000) >
 * calculateMagicalResistRate(...)` left to right, so the roll is drawn BEFORE the resist rate is computed. The target's attack-calc observer draws
 * when the rate asks it for RESIST (StatFunctions.calculateMagicalResistRate's first statement), so it must see the second value of the stream.
 * The element that decides the magical path is the template's field: the four-argument calculate ignores its element argument (:281).
 */
TEST_F(EffectTemplateTest, TheDodgeOrResistRollIsDrawnBeforeTheResistRateIsComputed) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2251);
	PlayerFixture target = makePlayer(2252);
	Ref<model::Effect> effect = effectFor(*caster.player, *target.player, bindSkill(ACTIVE_SKILL_XML), 1);
	Ref<DrawingObserver> observer = DrawingObserver::create();
	target.player->getObserveController()->addAttackCalcObserver(*observer);
	const std::vector<int32_t> rolls = rollStream(SEED, 2);
	ASSERT_NE(rolls[0], rolls[1]) << "the seed must tell the two draws apart";

	ProbeEffect magical;
	magical.element = gameserver::model::SkillElement::FIRE;
	Rnd::seedCurrentThreadForTests(SEED);
	(magical.*privateMember(CheckDodgeOrResistRateTag{}))(*effect);
	ASSERT_EQ(observer->drawn.size(), 1u) << "the magical path computes the resist rate once";
	EXPECT_EQ(observer->drawn[0], rolls[1]) << "the rate was computed after the roll";

	observer->drawn.clear();
	ProbeEffect physical; // element NONE: StatFunctions.checkIsDodgedHit, which never asks for RESIST
	Rnd::seedCurrentThreadForTests(SEED);
	physical.calculate(*effect, std::nullopt, std::nullopt, gameserver::model::SkillElement::FIRE);
	EXPECT_TRUE(observer->drawn.empty()) << "the element argument is ignored: the field's NONE takes the physical path";
	target.player->getObserveController()->removeAttackCalcObserver(*observer);
}

/**
 * Draw order kept by isDodgedOrResisted (:347-349): `!checkEffectResistRate(...) || !checkDodgeOrResistRate(...)` draws the effect-resist roll
 * first and skips the dodge-or-resist roll when the effect was resisted. checkEffectResistRate passes when its roll is at most the effect power,
 * 1000 - the abnormal resistance - the stat's resistance + the effector's penetration (:496-529); the target's STUN resistance puts the power
 * exactly at the first roll (passes) or one below it (resisted), which pins the arithmetic at its threshold as well.
 */
TEST_F(EffectTemplateTest, TheEffectResistRollComesFirstAndAResistedEffectDrawsNoDodgeRoll) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2261);
	PlayerFixture target = makePlayer(2262);
	Ref<model::Effect> effect = effectFor(*caster.player, *target.player, bindSkill(ACTIVE_SKILL_XML), 1);
	Ref<DrawingObserver> observer = DrawingObserver::create();
	target.player->getObserveController()->addAttackCalcObserver(*observer);
	const std::vector<int32_t> rolls = rollStream(SEED, 3);
	ASSERT_TRUE(rolls[0] != rolls[1] && rolls[1] != rolls[2] && rolls[0] != rolls[2]) << "the seed must tell the draws apart";
	ASSERT_GT(rolls[0], 1);

	// effect power = 1000 - abnormal resistance - STUN resistance + STUN penetration; the added resistance sets it to the first roll
	const int32_t basePower = 1000 - target.player->getGameStats()->getAbnormalResistance()->getCurrent()
		- target.player->getGameStats()->getResistance(StatEnum::STUN_RESISTANCE)->getCurrent()
		+ caster.player->getGameStats()->getStat(StatEnum::STUN_RESISTANCE_PENETRATION, 0)->getCurrent();
	addStat(*target.player, StatEnum::STUN_RESISTANCE, basePower - rolls[0]);
	ProbeEffect magical;
	magical.element = gameserver::model::SkillElement::FIRE;

	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE(magical.checkEffectResistRate(*effect, StatEnum::STUN_RESISTANCE)) << "power == roll: Rnd.get(1, 1000) <= effectPower";
	Rnd::seedCurrentThreadForTests(SEED);
	magical.isDodgedOrResisted(*effect, StatEnum::STUN_RESISTANCE);
	ASSERT_EQ(observer->drawn.size(), 1u) << "not resisted: the dodge-or-resist check ran";
	EXPECT_EQ(observer->drawn[0], rolls[2]) << "resist roll, dodge-or-resist roll, then the resist rate's observer";

	addStat(*target.player, StatEnum::STUN_RESISTANCE, 1); // power = roll - 1
	observer->drawn.clear();
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_FALSE(magical.checkEffectResistRate(*effect, StatEnum::STUN_RESISTANCE)) << "power == roll - 1: resisted";
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE(magical.isDodgedOrResisted(*effect, StatEnum::STUN_RESISTANCE));
	EXPECT_TRUE(observer->drawn.empty()) << "a resisted effect draws no dodge-or-resist roll";
	EXPECT_EQ(Rnd::get(1, 1000), rolls[1]) << "exactly one draw";
	target.player->getObserveController()->removeAttackCalcObserver(*observer);
}

/**
 * Java checkEffectResistRate (:492-494): no stat is no resist - true, before anything is read and without a roll. (The effect power behind a
 * stat is TheEffectResistRollComesFirstAndAResistedEffectDrawsNoDodgeRoll's threshold; calculateDamage's reflector arm needs a SKILL_REFLECTOR
 * AttackShieldObserver and belongs to the ReflectorEffect cases of part 3.)
 */
TEST_F(EffectTemplateTest, EffectResistRateWithoutAStatIsNoResist) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2301);
	PlayerFixture target = makePlayer(2302);
	Ref<model::Effect> effect = effectFor(*caster.player, *target.player, bindSkill(ACTIVE_SKILL_XML), 1);
	const std::vector<float> stream = chanceStream(SEED, 1);

	ProbeEffect probe;
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE(probe.checkEffectResistRate(*effect, std::nullopt));
	EXPECT_EQ(Rnd::chance(), stream[0]) << "no roll";
}

// ---- the private helpers ------------------------------------------------------------------------------------------------------------------------

/** Java isAlteredState (:551-556): every stat but BLEED_RESISTANCE and POISON_RESISTANCE (normal dots have no stat at all) */
TEST_F(EffectTemplateTest, EveryStatButBleedAndPoisonIsAnAlteredState) {
	ProbeEffect probe;
	const auto isAlteredState = privateMember(IsAlteredStateTag{});
	int altered = 0;
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<StatEnum>::names.size(); ++ordinal) {
		const StatEnum stat = static_cast<StatEnum>(ordinal);
		const bool expected = stat != StatEnum::BLEED_RESISTANCE && stat != StatEnum::POISON_RESISTANCE;
		EXPECT_EQ((probe.*isAlteredState)(stat), expected) << xml::enumName(stat);
		altered += (probe.*isAlteredState)(stat) ? 1 : 0;
	}
	EXPECT_EQ(altered, static_cast<int>(xml::EnumTraits<StatEnum>::names.size()) - 2);
}

/**
 * Java getPenetrationStat (:565-573): StatEnum.valueOf(name + "_PENETRATION"), null (and a warning) where no such constant exists. The 24
 * resistances of StatEnum.java:81-104 all have one (:107-130); the elemental resistances and the aggregate stats do not.
 */
TEST_F(EffectTemplateTest, PenetrationStatIsTheSameNamedPenetrationConstant) {
	ProbeEffect probe;
	const auto getPenetrationStat = privateMember(GetPenetrationStatTag{});
	const std::vector<std::pair<StatEnum, StatEnum>> pairs{
		{StatEnum::BLEED_RESISTANCE, StatEnum::BLEED_RESISTANCE_PENETRATION},
		{StatEnum::BLIND_RESISTANCE, StatEnum::BLIND_RESISTANCE_PENETRATION},
		{StatEnum::BIND_RESISTANCE, StatEnum::BIND_RESISTANCE_PENETRATION},
		{StatEnum::CHARM_RESISTANCE, StatEnum::CHARM_RESISTANCE_PENETRATION},
		{StatEnum::CONFUSE_RESISTANCE, StatEnum::CONFUSE_RESISTANCE_PENETRATION},
		{StatEnum::CURSE_RESISTANCE, StatEnum::CURSE_RESISTANCE_PENETRATION},
		{StatEnum::DISEASE_RESISTANCE, StatEnum::DISEASE_RESISTANCE_PENETRATION},
		{StatEnum::DEFORM_RESISTANCE, StatEnum::DEFORM_RESISTANCE_PENETRATION},
		{StatEnum::FEAR_RESISTANCE, StatEnum::FEAR_RESISTANCE_PENETRATION},
		{StatEnum::NOFLY_RESISTANCE, StatEnum::NOFLY_RESISTANCE_PENETRATION},
		{StatEnum::OPENAERIAL_RESISTANCE, StatEnum::OPENAERIAL_RESISTANCE_PENETRATION},
		{StatEnum::PARALYZE_RESISTANCE, StatEnum::PARALYZE_RESISTANCE_PENETRATION},
		{StatEnum::PERIFICATION_RESISTANCE, StatEnum::PERIFICATION_RESISTANCE_PENETRATION},
		{StatEnum::POISON_RESISTANCE, StatEnum::POISON_RESISTANCE_PENETRATION},
		{StatEnum::PULLED_RESISTANCE, StatEnum::PULLED_RESISTANCE_PENETRATION},
		{StatEnum::ROOT_RESISTANCE, StatEnum::ROOT_RESISTANCE_PENETRATION},
		{StatEnum::SILENCE_RESISTANCE, StatEnum::SILENCE_RESISTANCE_PENETRATION},
		{StatEnum::SLEEP_RESISTANCE, StatEnum::SLEEP_RESISTANCE_PENETRATION},
		{StatEnum::SLOW_RESISTANCE, StatEnum::SLOW_RESISTANCE_PENETRATION},
		{StatEnum::SNARE_RESISTANCE, StatEnum::SNARE_RESISTANCE_PENETRATION},
		{StatEnum::SPIN_RESISTANCE, StatEnum::SPIN_RESISTANCE_PENETRATION},
		{StatEnum::STAGGER_RESISTANCE, StatEnum::STAGGER_RESISTANCE_PENETRATION},
		{StatEnum::STUMBLE_RESISTANCE, StatEnum::STUMBLE_RESISTANCE_PENETRATION},
		{StatEnum::STUN_RESISTANCE, StatEnum::STUN_RESISTANCE_PENETRATION},
	};
	LogCapture warnings("com.aionemu.gameserver.skillengine.effect.EffectTemplate");
	for (const auto& [resistance, penetration] : pairs)
		EXPECT_EQ((probe.*getPenetrationStat)(resistance), penetration) << xml::enumName(resistance);
	EXPECT_EQ(warnings.text(), "") << "a stat with a penetration constant logs nothing";

	for (StatEnum none : {StatEnum::FIRE_RESISTANCE, StatEnum::STUNLIKE_RESISTANCE, StatEnum::ABNORMAL_RESISTANCE_ALL, StatEnum::MAXHP,
			 StatEnum::STUN_RESISTANCE_PENETRATION})
		EXPECT_EQ((probe.*getPenetrationStat)(none), std::nullopt) << xml::enumName(none);
	EXPECT_EQ(warnings.text(), "warning|Missing statenum penetration for FIRE_RESISTANCE\n"
							   "warning|Missing statenum penetration for STUNLIKE_RESISTANCE\n"
							   "warning|Missing statenum penetration for ABNORMAL_RESISTANCE_ALL\n"
							   "warning|Missing statenum penetration for MAXHP\n"
							   "warning|Missing statenum penetration for STUN_RESISTANCE_PENETRATION\n")
		<< "Java logs one warning per missing constant, naming the stat (EffectTemplate.java:570)";
}

/**
 * Java isProtectedByShield (:558-563): only STUMBLE, OPENAERIAL, SPIN and STAGGER ask the effect controller whether the effected is under a
 * normal shield (EffectController.isUnderNormalShield: an abnormal effect whose shield defense has the NORMAL bit); every other stat answers
 * false without asking - so with a normal shield up exactly the four answer true.
 */
TEST_F(EffectTemplateTest, OnlyTheFourKnockStatsAskForANormalShield) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture target = makePlayer(2401);
	ProbeEffect probe;
	const auto isProtectedByShield = privateMember(IsProtectedByShieldTag{});
	auto protectedStats = [&] {
		std::vector<StatEnum> stats;
		for (size_t ordinal = 0; ordinal < xml::EnumTraits<StatEnum>::names.size(); ++ordinal) {
			const StatEnum stat = static_cast<StatEnum>(ordinal);
			if ((probe.*isProtectedByShield)(*target.player, stat))
				stats.push_back(stat);
		}
		return stats;
	};
	EXPECT_TRUE(protectedStats().empty()) << "no shield";

	Ref<model::Effect> shield = effectFor(*target.player, *target.player, bindSkill(ACTIVE_SKILL_XML), 1);
	shield->setShieldDefense(2); // ShieldType.NORMAL
	target.player->getEffectController()->addEffect(*shield);
	ASSERT_TRUE(target.player->getEffectController()->isUnderNormalShield());
	EXPECT_EQ(protectedStats(), (std::vector<StatEnum>{StatEnum::OPENAERIAL_RESISTANCE, StatEnum::SPIN_RESISTANCE, StatEnum::STAGGER_RESISTANCE,
									StatEnum::STUMBLE_RESISTANCE}))
		<< "the four knock stats, in StatEnum order";
	target.player->getEffectController()->clearEffectMapsWithoutNotify(); // the never started effect holds the player
}

/**
 * Java validatePreEffects (:335-345): no preeffect list passes without a roll; with one, every listed position must have succeeded
 * (Effect.isInSuccessEffects) - a missing one fails before the roll - and then a chance roll below preeffect_prob decides. An empty list is the
 * roll alone.
 */
TEST_F(EffectTemplateTest, PreEffectsNeedTheirPositionsAndThePreEffectProbabilityRoll) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2501);
	Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, bindSkill(ACTIVE_SKILL_XML), 1);
	const auto validatePreEffects = privateMember(ValidatePreEffectsTag{});
	const std::vector<float> stream = chanceStream(SEED, 2);
	ASSERT_GT(stream[0], 1.0f);
	ASSERT_LT(stream[0], 99.0f);

	ProbeEffect probe;
	probe.preEffectProb = 0;
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE((probe.*validatePreEffects)(*effect)) << "no preeffect attribute: preeffect_prob is not even read";
	EXPECT_EQ(Rnd::chance(), stream[0]) << "no roll";

	probe.preEffects = std::vector<int32_t>{};
	probe.preEffectProb = static_cast<int32_t>(stream[0]); // roll >= prob: fails
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_FALSE((probe.*validatePreEffects)(*effect)) << "roll " << stream[0] << " >= preeffect_prob " << probe.preEffectProb;
	EXPECT_EQ(Rnd::chance(), stream[1]) << "one roll";
	probe.preEffectProb = static_cast<int32_t>(stream[0]) + 1; // roll < prob: passes
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE((probe.*validatePreEffects)(*effect)) << "roll " << stream[0] << " < preeffect_prob " << probe.preEffectProb;
	probe.preEffectProb = 100;
	EXPECT_TRUE((probe.*validatePreEffects)(*effect)) << "the default 100 always passes";

	ProbeEffect positionOne;
	positionOne.position = 1;
	probe.preEffects = std::vector<int32_t>{1, 2};
	effect->addSuccessEffect(&positionOne);
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_FALSE((probe.*validatePreEffects)(*effect)) << "position 2 did not succeed";
	EXPECT_EQ(Rnd::chance(), stream[0]) << "a missing position fails before the roll";
	probe.preEffects = std::vector<int32_t>{1};
	probe.preEffectProb = static_cast<int32_t>(stream[0]);
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_FALSE((probe.*validatePreEffects)(*effect)) << "position 1 succeeded, then the roll decides: " << stream[0];
	EXPECT_EQ(Rnd::chance(), stream[1]) << "one roll";
	probe.preEffectProb = static_cast<int32_t>(stream[0]) + 1;
	Rnd::seedCurrentThreadForTests(SEED);
	EXPECT_TRUE((probe.*validatePreEffects)(*effect));
}

/**
 * Java validateEffectConditions (:331-333) and effectSubConditionsCheck (:439-441) over Conditions.validate(Effect) (Conditions.java:66-73): no
 * list passes, otherwise every condition must pass and the first failure ends the loop. Each reads its own list.
 */
TEST_F(EffectTemplateTest, EffectConditionsAndSubConditionsPassOnlyWhenEveryConditionPasses) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2601);
	Ref<model::Effect> effect = effectFor(*caster.player, *caster.player, bindSkill(ACTIVE_SKILL_XML), 1);
	const auto validateEffectConditions = privateMember(ValidateEffectConditionsTag{});
	const auto effectSubConditionsCheck = privateMember(EffectSubConditionsCheckTag{});

	ProbeEffect probe;
	EXPECT_TRUE((probe.*validateEffectConditions)(*effect)) << "no <conditions>";
	EXPECT_TRUE((probe.*effectSubConditionsCheck)(*effect)) << "no <subconditions>";

	ConditionProbes passing = conditionProbes({true, true});
	probe.effectConditions = std::move(passing.conditions);
	EXPECT_TRUE((probe.*validateEffectConditions)(*effect));
	EXPECT_EQ(passing.probes[0]->calls, 1);
	EXPECT_EQ(passing.probes[1]->calls, 1);
	EXPECT_TRUE((probe.*effectSubConditionsCheck)(*effect)) << "the sub check reads <subconditions>, which is still empty";

	ConditionProbes failing = conditionProbes({true, false, true});
	probe.effectSubConditions = std::move(failing.conditions);
	EXPECT_FALSE((probe.*effectSubConditionsCheck)(*effect));
	EXPECT_EQ(failing.probes[0]->calls, 1);
	EXPECT_EQ(failing.probes[1]->calls, 1);
	EXPECT_EQ(failing.probes[2]->calls, 0) << "the first failing condition ends the loop";
	EXPECT_TRUE((probe.*validateEffectConditions)(*effect)) << "the effect conditions do not read <subconditions>";
	EXPECT_EQ(passing.probes[0]->calls, 2);
}

/**
 * The private helpers over the Effect: addSuccessEffect adds the template under its position and sets a given spell status (:363-367);
 * isImmuneToAbnormal only considers an npc or summon other than the effector (:524-541); and checkDodgeOrResistRate's accuracy modifier -
 * accmod2 + accmod1 * skillLevel + the effect's accuracy boost, plus the effector's BOOST_RESIST_DEBUFF for a DEBUFF skill (:351-357) - goes into
 * the magical resist rate, min(500, mResist - mAccuracy - accuracyModifier) between two players, which the roll must exceed. The accuracy
 * modifier puts the rate exactly at the roll (resisted: `roll > rate` is false) and one below it (passes).
 */
TEST_F(EffectTemplateTest, TheHelpersAddTheSuccessDecideTheImmunityAndWeighTheAccuracy) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture caster = makePlayer(2701);
	PlayerFixture target = makePlayer(2702);
	Ref<gameserver::model::gameobjects::Npc> rooted = makeRootedNpc(700271);
	const model::SkillTemplate* skill = bindSkill(ACTIVE_SKILL_XML);
	Ref<model::Effect> effect = effectFor(*caster.player, *target.player, skill, 3);
	ProbeEffect probe;
	probe.position = 2;

	(probe.*privateMember(AddSuccessEffectTag{}))(*effect, std::nullopt);
	EXPECT_EQ(effect->effectInPos(2), &probe);
	EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::NONE) << "no spell status given: unchanged";
	(probe.*privateMember(AddSuccessEffectTag{}))(*effect, model::SpellStatus::STUMBLE);
	EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::STUMBLE);

	const auto isImmuneToAbnormal = privateMember(IsImmuneToAbnormalTag{});
	EXPECT_FALSE((probe.*isImmuneToAbnormal)(*effect, StatEnum::STAGGER_RESISTANCE)) << "a player is never immune here";
	Ref<model::Effect> onRooted = effectFor(*caster.player, *rooted, skill, 1);
	EXPECT_TRUE((probe.*isImmuneToAbnormal)(*onRooted, StatEnum::STAGGER_RESISTANCE));
	EXPECT_TRUE((probe.*isImmuneToAbnormal)(*onRooted, StatEnum::PULLED_RESISTANCE));
	EXPECT_FALSE((probe.*isImmuneToAbnormal)(*onRooted, StatEnum::STUN_RESISTANCE));
	Ref<model::Effect> ownEffect = effectFor(*rooted, *rooted, skill, 1);
	EXPECT_FALSE((probe.*isImmuneToAbnormal)(*ownEffect, StatEnum::STAGGER_RESISTANCE)) << "the npc's own effect";

	uint64_t seed = SEED;
	while (rollStream(seed, 1)[0] >= 500) // a roll below the PvP clamp of 500
		++seed;
	const int32_t roll = rollStream(seed, 1)[0];
	const int32_t mResist = target.player->getGameStats()->getMResist()->getCurrent();
	const int32_t mAccuracy = caster.player->getGameStats()->getMAccuracy()->getCurrent();
	const int32_t boostResistDebuff = caster.player->getGameStats()->getStat(StatEnum::BOOST_RESIST_DEBUFF, 0)->getCurrent();
	ProbeEffect magical;
	magical.element = gameserver::model::SkillElement::WIND;
	magical.accMod1 = 10;
	effect->setAccModBoost(7);
	// rate = mResist - mAccuracy - (accmod2 + 10 * 3 + 7 + boostResistDebuff) == roll
	magical.accMod2 = mResist - mAccuracy - roll - 30 - 7 - boostResistDebuff;
	const auto checkDodgeOrResistRate = privateMember(CheckDodgeOrResistRateTag{});
	Rnd::seedCurrentThreadForTests(seed);
	EXPECT_FALSE((magical.*checkDodgeOrResistRate)(*effect)) << "rate == roll " << roll << ": resisted";
	magical.accMod2 += 1;
	Rnd::seedCurrentThreadForTests(seed);
	EXPECT_TRUE((magical.*checkDodgeOrResistRate)(*effect)) << "rate == roll - 1: not resisted";
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::test
