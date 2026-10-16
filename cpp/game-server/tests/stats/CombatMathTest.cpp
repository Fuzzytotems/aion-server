// P5-01 (M5b-1, work items B-02, B-03, B-05 and B-08): the combat tables that need no game object - the AttackStatus companion
// (AttackStatus.java), the XPRewardEnum companion (XPRewardEnum.java) and the two npc rating switches of StatFunctions - and the two bodies
// that need only an npc: StatFunctions.calculateHate (StatFunctions.java:267-270) and NpcGameStats.getNextAttackInterval /
// getInitialSkillDelay (NpcGameStats.java:142-158, 226).
//
// Every expectation is the Java value of the cited source line, not a value read back from the port.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/controllers/attack/KillCounter.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/utils/stats/XPRewardEnum.h"
#include "aion/gameserver/utils/stats/XPRewardEnumInfo.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "StatsTestSupport.h"

namespace aion::gameserver::model::stats::test {
namespace {

using controllers::attack::AttackStatus;
using runtime::Ptr;
using runtime::Ref;
using templates::npc::NpcRating;
using utils::stats::StatFunctions;
using utils::stats::XPRewardEnum;

/** A spawn template of the group, like the spawn data of a map */
class CombatSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit CombatSpawnTemplate(templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

class CombatMathTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 41));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
	}

	void TearDown() override {
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		groups.clear();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
	const templates::npc::NpcTemplate* npcTemplate(std::string_view attributes, std::string_view children) {
		xml::LoadContext context;
		return xml::bindString<templates::npc::NpcTemplate>(
			context, "<npc_template name_id=\"1\" " + std::string(attributes) + ">" + std::string(children) + "</npc_template>")
			.release();
	}

	Ref<gameobjects::Npc> makeNpc(const templates::npc::NpcTemplate* objectTemplate) {
		Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(210010000, 700001, 0, nullptr);
		templates::spawns::SpawnTemplate& spawnTemplate = group->addSpawnTemplate(std::make_unique<CombatSpawnTemplate>(*group));
		groups.push_back(group);
		Ref<gameobjects::Npc> npc = gameobjects::VisibleObject::create<gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawnTemplate, objectTemplate);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		return npc;
	}

	runtime::ManualClock clock{0};
	std::vector<Ref<templates::spawns::SpawnGroup>> groups;
};

// --------------------------------------------------------------------------------------------- AttackStatus (the companion of P5-01)

TEST_F(CombatMathTest, AttackStatusDecisionTable) {
	// AttackStatus.java:6-27 - the wire id, the counterSkill flag and the isCritical flag of every constant, in ordinal order
	struct Row {
		AttackStatus status;
		int32_t id;
		bool counterSkill;
		bool critical;
	};
	const Row rows[] = {
		{AttackStatus::DODGE, 0, true, false},
		{AttackStatus::OFFHAND_DODGE, 1, true, false},
		{AttackStatus::PARRY, 2, true, false},
		{AttackStatus::OFFHAND_PARRY, 3, true, false},
		{AttackStatus::BLOCK, 4, true, false},
		{AttackStatus::OFFHAND_BLOCK, 5, true, false},
		{AttackStatus::RESIST, 6, true, false},
		{AttackStatus::OFFHAND_RESIST, 7, true, false},
		{AttackStatus::BUF, 8, false, false},
		{AttackStatus::OFFHAND_BUF, 9, false, false},
		{AttackStatus::NORMALHIT, 10, false, false},
		{AttackStatus::OFFHAND_NORMALHIT, 11, false, false},
		{AttackStatus::CRITICAL_DODGE, -64, true, true},
		{AttackStatus::CRITICAL_PARRY, -62, true, true},
		{AttackStatus::CRITICAL_BLOCK, -60, true, true},
		{AttackStatus::CRITICAL_RESIST, -58, true, true},
		{AttackStatus::CRITICAL, -54, false, true},
		{AttackStatus::OFFHAND_CRITICAL_DODGE, -47, true, true},
		{AttackStatus::OFFHAND_CRITICAL_PARRY, -45, true, true},
		{AttackStatus::OFFHAND_CRITICAL_BLOCK, -43, true, true},
		{AttackStatus::OFFHAND_CRITICAL_RESIST, -41, true, true},
		{AttackStatus::OFFHAND_CRITICAL, -37, false, true},
	};
	for (const Row& row : rows) {
		EXPECT_EQ(getId(row.status), row.id) << getName(row.status);
		EXPECT_EQ(isCounterSkill(row.status), row.counterSkill) << getName(row.status);
		EXPECT_EQ(isCritical(row.status), row.critical) << getName(row.status);
	}

	// AttackStatus.getBaseStatus: the four counter statuses collapse, everything else is itself
	for (AttackStatus dodge : {AttackStatus::DODGE, AttackStatus::CRITICAL_DODGE, AttackStatus::OFFHAND_DODGE, AttackStatus::OFFHAND_CRITICAL_DODGE})
		EXPECT_EQ(getBaseStatus(dodge), AttackStatus::DODGE) << getName(dodge);
	for (AttackStatus resist :
		{AttackStatus::RESIST, AttackStatus::CRITICAL_RESIST, AttackStatus::OFFHAND_RESIST, AttackStatus::OFFHAND_CRITICAL_RESIST})
		EXPECT_EQ(getBaseStatus(resist), AttackStatus::RESIST) << getName(resist);
	for (AttackStatus parry : {AttackStatus::PARRY, AttackStatus::CRITICAL_PARRY, AttackStatus::OFFHAND_PARRY, AttackStatus::OFFHAND_CRITICAL_PARRY})
		EXPECT_EQ(getBaseStatus(parry), AttackStatus::PARRY) << getName(parry);
	for (AttackStatus block : {AttackStatus::BLOCK, AttackStatus::CRITICAL_BLOCK, AttackStatus::OFFHAND_BLOCK, AttackStatus::OFFHAND_CRITICAL_BLOCK})
		EXPECT_EQ(getBaseStatus(block), AttackStatus::BLOCK) << getName(block);
	EXPECT_EQ(getBaseStatus(AttackStatus::CRITICAL), AttackStatus::CRITICAL);
	EXPECT_EQ(getBaseStatus(AttackStatus::NORMALHIT), AttackStatus::NORMALHIT);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_NORMALHIT), AttackStatus::OFFHAND_NORMALHIT);
	EXPECT_EQ(getBaseStatus(AttackStatus::BUF), AttackStatus::BUF);

	// AttackStatus.getOffHandStats: the eleven main-hand statuses map, every other constant throws
	EXPECT_EQ(getOffHandStats(AttackStatus::DODGE), AttackStatus::OFFHAND_DODGE);
	EXPECT_EQ(getOffHandStats(AttackStatus::PARRY), AttackStatus::OFFHAND_PARRY);
	EXPECT_EQ(getOffHandStats(AttackStatus::BLOCK), AttackStatus::OFFHAND_BLOCK);
	EXPECT_EQ(getOffHandStats(AttackStatus::RESIST), AttackStatus::OFFHAND_RESIST);
	EXPECT_EQ(getOffHandStats(AttackStatus::BUF), AttackStatus::OFFHAND_BUF);
	EXPECT_EQ(getOffHandStats(AttackStatus::NORMALHIT), AttackStatus::OFFHAND_NORMALHIT);
	EXPECT_EQ(getOffHandStats(AttackStatus::CRITICAL), AttackStatus::OFFHAND_CRITICAL);
	EXPECT_EQ(getOffHandStats(AttackStatus::CRITICAL_DODGE), AttackStatus::OFFHAND_CRITICAL_DODGE);
	EXPECT_EQ(getOffHandStats(AttackStatus::CRITICAL_PARRY), AttackStatus::OFFHAND_CRITICAL_PARRY);
	EXPECT_EQ(getOffHandStats(AttackStatus::CRITICAL_BLOCK), AttackStatus::OFFHAND_CRITICAL_BLOCK);
	EXPECT_EQ(getOffHandStats(AttackStatus::CRITICAL_RESIST), AttackStatus::OFFHAND_CRITICAL_RESIST);
	EXPECT_THROW(static_cast<void>(getOffHandStats(AttackStatus::OFFHAND_DODGE)), runtime::IllegalArgumentException)
		<< "Java: the switch has no default, so an off-hand status falls through to throw new IllegalArgumentException";
	EXPECT_THROW(static_cast<void>(getOffHandStats(AttackStatus::OFFHAND_CRITICAL)), runtime::IllegalArgumentException);

	// AttackStatus.getCriticalStatusFor
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::DODGE), AttackStatus::CRITICAL_DODGE);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::OFFHAND_DODGE), AttackStatus::OFFHAND_CRITICAL_DODGE);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::PARRY), AttackStatus::CRITICAL_PARRY);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::OFFHAND_PARRY), AttackStatus::OFFHAND_CRITICAL_PARRY);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::BLOCK), AttackStatus::CRITICAL_BLOCK);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::OFFHAND_BLOCK), AttackStatus::OFFHAND_CRITICAL_BLOCK);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::NORMALHIT), AttackStatus::CRITICAL);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::OFFHAND_NORMALHIT), AttackStatus::OFFHAND_CRITICAL);
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::RESIST), AttackStatus::RESIST) << "Java: default -> status";
	EXPECT_EQ(getCriticalStatusFor(AttackStatus::CRITICAL), AttackStatus::CRITICAL);
}

// ----------------------------------------------------------------------------------------- XPRewardEnum (the reward percentage table)

TEST_F(CombatMathTest, XpRewardPercentageTable) {
	// XPRewardEnum.java:9-24, in ordinal order: the level difference and its reward percentage
	const std::pair<int32_t, int32_t> rows[] = {{-11, 0}, {-10, 1}, {-9, 10}, {-8, 20}, {-7, 30}, {-6, 40}, {-5, 50}, {-4, 60}, {-3, 90},
		{-2, 100}, {-1, 100}, {0, 100}, {1, 105}, {2, 110}, {3, 115}, {4, 120}};
	for (const auto& [diff, percent] : rows)
		EXPECT_EQ(utils::stats::xpRewardFrom(diff), percent) << "level difference " << diff;

	// the two clamps of xpRewardFrom (XPRewardEnum.java:44-49)
	EXPECT_EQ(utils::stats::xpRewardFrom(-12), 0) << "below MINUS_11 -> MINUS_11's percentage";
	EXPECT_EQ(utils::stats::xpRewardFrom(-1000), 0);
	EXPECT_EQ(utils::stats::xpRewardFrom(5), 120) << "above PLUS_4 -> PLUS_4's percentage";
	EXPECT_EQ(utils::stats::xpRewardFrom(1000), 120);

	EXPECT_EQ(utils::stats::rewardPercent(XPRewardEnum::PLUS_1), 105);
	EXPECT_EQ(utils::stats::levelDifference(XPRewardEnum::PLUS_1), 1);
}

// -------------------------------------------------------------------------------------------- the two npc rating switches of StatFunctions

TEST_F(CombatMathTest, RatingMultiplierAndApRating) {
	// StatFunctions.java:435-450
	EXPECT_EQ(StatFunctions::calculateRatingMultiplier(NpcRating::JUNK), 2);
	EXPECT_EQ(StatFunctions::calculateRatingMultiplier(NpcRating::NORMAL), 2);
	EXPECT_EQ(StatFunctions::calculateRatingMultiplier(NpcRating::ELITE), 3);
	EXPECT_EQ(StatFunctions::calculateRatingMultiplier(NpcRating::HERO), 4);
	EXPECT_EQ(StatFunctions::calculateRatingMultiplier(NpcRating::LEGENDARY), 5);

	// StatFunctions.java:452-467
	EXPECT_EQ(StatFunctions::getApNpcRating(NpcRating::JUNK), 1);
	EXPECT_EQ(StatFunctions::getApNpcRating(NpcRating::NORMAL), 2);
	EXPECT_EQ(StatFunctions::getApNpcRating(NpcRating::ELITE), 4);
	EXPECT_EQ(StatFunctions::getApNpcRating(NpcRating::HERO), 35);
	EXPECT_EQ(StatFunctions::getApNpcRating(NpcRating::LEGENDARY), 2500);
}

// --------------------------------------------------------------------------------------------------------- StatFunctions.calculateHate

TEST_F(CombatMathTest, HateIsTheBoostHateShareOfTheValue) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameobjects::Npc> npc = makeNpc(npcTemplate(R"(npc_id="700001" level="2" name="hater" attack_speed="2142" rating="NORMAL" rank="DISCIPLINED")",
		R"(<stats maxHp="199" maxMp="0" attack="16"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats>)"));

	// Java: getStat(BOOST_HATE, 100).getCurrent() is 100 without a boost effect, so hate = (int) ((long) value * 1100 / 1000)
	ASSERT_EQ(npc->getGameStats()->getStat(container::StatEnum::BOOST_HATE, 100)->getCurrent(), 100);
	EXPECT_EQ(StatFunctions::calculateHate(*npc, 0), 0);
	EXPECT_EQ(StatFunctions::calculateHate(*npc, 1), 1) << "1 * 1100 / 1000 truncates to 1";
	EXPECT_EQ(StatFunctions::calculateHate(*npc, 10), 11);
	EXPECT_EQ(StatFunctions::calculateHate(*npc, 500), 550) << "the hate of a 50 damage auto attack (AggroList multiplies the damage by 10)";
	EXPECT_EQ(StatFunctions::calculateHate(*npc, 1990), 2189) << "1990 * 1100 = 2189000, / 1000 = 2189";
	EXPECT_EQ(StatFunctions::calculateHate(*npc, -10), -11) << "a negative value keeps its sign (Java truncates towards zero)";

	// Java `(int) ((long) value * (1000 + boostHate) / 1000)`: the product is a long, and the (int) cast of a long keeps the low 32 bits
	// (it does not saturate - only a float/double cast does), so 2200000000 wraps to 2200000000 - 2^32
	EXPECT_EQ(StatFunctions::calculateHate(*npc, 2000000000), -2094967296)
		<< "2000000000 * 1100 / 1000 = 2200000000L, and (int) 2200000000L is -2094967296";
}

// ------------------------------------------------------------------------------------- NpcGameStats.getNextAttackInterval / getInitialSkillDelay

TEST_F(CombatMathTest, NextAttackIntervalIsTheRestOfTheAttackSpeed) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameobjects::Npc> npc = makeNpc(npcTemplate(R"(npc_id="700001" level="2" name="sparkie" attack_speed="2142" rating="NORMAL" rank="DISCIPLINED")",
		R"(<stats maxHp="199" maxMp="0" attack="16"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats>)"));
	Ptr<container::NpcGameStats> stats = npc->getGameStats();
	ASSERT_EQ(stats->getAttackSpeed()->getCurrent(), 2142);

	// lastAttackTime == 0 and no target: attackDelay is the whole wall clock, so neither branch fires
	EXPECT_EQ(stats->getNextAttackInterval(), 0) << "no opener without a target, and the delay is longer than the attack speed";

	// after an attack the interval is what is left of the attack speed
	stats->renewLastAttackTime();
	int32_t interval = stats->getNextAttackInterval();
	EXPECT_GT(interval, 2000) << "attackSpeed - attackDelay, measured against the wall clock";
	EXPECT_LE(interval, 2142);

	stats->resetFightStats();
	EXPECT_EQ(stats->getNextAttackInterval(), 0) << "resetFightStats puts lastAttackTime back to 0";
}

TEST_F(CombatMathTest, NextAttackIntervalFallsBackToTwoSecondsWithoutAnAttackSpeed) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Java: `if (attackSpeed == 0) attackSpeed = 2000;` (NpcGameStats.java:145-147)
	Ref<gameobjects::Npc> npc = makeNpc(npcTemplate(R"(npc_id="700002" level="2" name="slow" attack_speed="0" rating="NORMAL" rank="NOVICE")",
		R"(<stats maxHp="199" maxMp="0" attack="16"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats>)"));
	Ptr<container::NpcGameStats> stats = npc->getGameStats();
	ASSERT_EQ(stats->getAttackSpeed()->getCurrent(), 0);

	stats->renewLastAttackTime();
	int32_t interval = stats->getNextAttackInterval();
	EXPECT_GT(interval, 1900);
	EXPECT_LE(interval, 2000) << "the 2000 ms fallback, not the template's 0";
}

TEST_F(CombatMathTest, InitialSkillDelayIsBetweenOneAndThreeAttackSpeeds) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameobjects::Npc> npc = makeNpc(npcTemplate(R"(npc_id="700001" level="2" name="sparkie" attack_speed="2142" rating="NORMAL" rank="DISCIPLINED")",
		R"(<stats maxHp="199" maxMp="0" attack="16"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats>)"));
	Ptr<container::NpcGameStats> stats = npc->getGameStats();

	// Java: owner.getAi().modifyInitialSkillDelay(Rnd.get(attackSpeed, 3 * attackSpeed)) - the default AI returns the delay unchanged
	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	const int32_t first = stats->getInitialSkillDelay();
	EXPECT_GE(first, 2142);
	EXPECT_LE(first, 6426);

	// the same seed replays the same sequence, so the bounds are the whole content of the body
	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	EXPECT_EQ(stats->getInitialSkillDelay(), first);

	bool sawMoreThanOneValue = false;
	for (int i = 0; i < 40; i++) {
		int32_t delay = stats->getInitialSkillDelay();
		EXPECT_GE(delay, 2142) << "Rnd.get(attackSpeed, 3 * attackSpeed) is inclusive on both ends";
		EXPECT_LE(delay, 6426);
		sawMoreThanOneValue |= delay != first;
	}
	EXPECT_TRUE(sawMoreThanOneValue) << "a constant would pass the bounds but is not what Java returns";
}

// ------------------------------------------------------------------------------------------------------- KillCounter (B-04, new file)

TEST_F(CombatMathTest, KillCounterCountsOnlyTheKillsOfTheConfiguredPeriod) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// KillCounter.addKillFor (KillCounter.java:22-32): the kill times of one victim, pruned to the past CustomConfig.PVP_DAY_DURATION.
	// M5b-1 has no caller (PvpService is P5-08 and unported), so this is the only coverage the body gets.
	AtomicConfigScope<int64_t> dayDuration(configs::main::CustomConfig::PVP_DAY_DURATION, int64_t{24} * 60 * 60 * 1000);

	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1001, 2001), 1);
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1001, 2001), 2) << "the same pair counts up";
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1001, 2001), 3);
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1001, 2002), 1) << "another victim has its own list";
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1002, 2001), 1) << "another killer has its own map";
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1001, 2001), 4);

	// the pruning: with a negative period `now - PVP_DAY_DURATION` lies in the future, so every stored kill time is below it and is dropped
	// (a period of 0 would be timing dependent, because two kills in the same millisecond have equal times)
	AtomicConfigScope<int64_t> noDuration(configs::main::CustomConfig::PVP_DAY_DURATION, int64_t{-1000});
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1003, 2003), 1);
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1003, 2003), 1) << "removeIf(time < minAge) drops the previous kill";
	EXPECT_EQ(controllers::attack::KillCounter::addKillFor(1001, 2001), 1) << "the four kills of the first pair are pruned too";
}

} // namespace
} // namespace aion::gameserver::model::stats::test
