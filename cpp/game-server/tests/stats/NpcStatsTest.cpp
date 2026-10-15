// The NPC side of P5-01 (wave 5a, work items B-01 and B-03): NpcGameStats over the npc template's stats and the NpcLifeStats the spawn creates
// (Npc::setupStatContainers). The npc template is bound from XML text in the Java format of npc_templates.xml; expectations follow the Java sources.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::model::stats::test {
namespace {

using container::StatEnum;
using runtime::Ptr;
using runtime::Ref;

/** A spawn template of the group, like the spawn data of a map */
class NpcStatsSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit NpcStatsSpawnTemplate(templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
const templates::npc::NpcTemplate* npcTemplate(std::string_view attributes, std::string_view children) {
	xml::LoadContext context;
	return xml::bindString<templates::npc::NpcTemplate>(
		context, "<npc_template name_id=\"1\" " + std::string(attributes) + ">" + std::string(children) + "</npc_template>")
		.release();
}

class NpcStatsTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 9));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
	}

	void TearDown() override {
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
};

TEST_F(NpcStatsTest, GameAndLifeStatsOfASpawnedNpc) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const templates::npc::NpcTemplate* objectTemplate =
		npcTemplate(R"(npc_id="700001" level="4" name="Test" attack_speed="2000" arange="2" rating="NORMAL" tribe="GENERAL")",
			R"(<stats maxHp="2522" maxMp="100" pdef="130" mdef="70" attack="16" evasion="45" parry="30" block="25" accuracy="200" macc="60" pcrit="10" mcrit="20">)"
			R"(<speeds walk="0.8" run="2.0" run_fight="3.0" group_walk="0.5" group_run_fight="2.5" fly="4.0"/></stats>)");
	Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(210010000, 700001, 0, nullptr);
	templates::spawns::SpawnTemplate& spawnTemplate =
		group->addSpawnTemplate(std::make_unique<NpcStatsSpawnTemplate>(*group));
	Ref<gameobjects::Npc> npc = gameobjects::VisibleObject::create<gameobjects::Npc>(std::make_unique<controllers::NpcController>(), spawnTemplate,
		objectTemplate);
	// Java: SpawnEngine gives every spawned npc a NpcKnownList (the HP broadcasts read it)
	npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));

	Ptr<container::NpcGameStats> stats = npc->getGameStats();
	Ptr<container::NpcLifeStats> life = npc->getLifeStats();
	ASSERT_TRUE(stats);
	ASSERT_TRUE(life);
	EXPECT_EQ(stats->getStatsTemplate(), objectTemplate->getStatsTemplate());
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 2522);
	EXPECT_EQ(stats->getMaxMp()->getCurrent(), 100);
	EXPECT_EQ(stats->getPDef()->getCurrent(), 130);
	EXPECT_EQ(stats->getMDef()->getCurrent(), 70);
	EXPECT_EQ(stats->getEvasion()->getCurrent(), 45);
	EXPECT_EQ(stats->getParry()->getCurrent(), 30) << "the parry of an NPC is the template value (cap [0, unlimited])";
	EXPECT_EQ(stats->getBaseAttackSpeed(), 2000);
	EXPECT_EQ(stats->getAttackSpeed()->getCurrent(), 2000);
	EXPECT_EQ(stats->getAttackRange()->getCurrent(), 2000) << "attack_range 2 * 1000";
	// not in fight and not walking: run speed * 1000
	EXPECT_EQ(stats->getMovementSpeed()->getCurrent(), 2000);
	EXPECT_FLOAT_EQ(stats->getMovementSpeedFloat(), 2.0f);
	npc->setState(gameobjects::state::CreatureState::WALK_MODE);
	EXPECT_EQ(stats->getMovementSpeed()->getCurrent(), 800) << "walk speed of a single npc";
	npc->unsetState(gameobjects::state::CreatureState::WALK_MODE);
	npc->setState(gameobjects::state::CreatureState::WEAPON_EQUIPPED);
	EXPECT_EQ(stats->getMovementSpeed()->getCurrent(), 3000) << "run speed in fight";
	npc->unsetState(gameobjects::state::CreatureState::WEAPON_EQUIPPED);
	// Java: maxHp / 2 (4 for abyss npcs)
	EXPECT_EQ(stats->getHpRegenRate()->getCurrent(), 1261);
	EXPECT_THROW(static_cast<void>(stats->getMpRegenRate()), runtime::IllegalStateException) << "No mp regen for NPC";

	// NpcLifeStats(Npc): the max values of the moment
	EXPECT_EQ(life->getCurrentHp(), 2522);
	EXPECT_EQ(life->getCurrentMp(), 100);
	EXPECT_FALSE(life->isDead());
	EXPECT_EQ(life->getHpPercentage(), 100);
	EXPECT_EQ(life->getMaxFp(), 0) << "only players have flight points";
	EXPECT_EQ(life->getCurrentFp(), 0);
	life->setCurrentHpPercent(50);
	EXPECT_EQ(life->getCurrentHp(), 1261);
	EXPECT_EQ(life->getHpPercentage(), 50);

	// fight timers (all of them start at 0)
	EXPECT_EQ(stats->getCastSpeed(), 1000) << "the template default cast_speed";
	EXPECT_FALSE(stats->isNextAttackScheduled());
	EXPECT_TRUE(stats->canUseNextSkill());
	stats->renewLastAttackTime();
	stats->renewLastChangeTargetTime();
	EXPECT_EQ(stats->getLastAttackTimeDelta(), 0);
	EXPECT_EQ(stats->getLastChangeTargetTimeDelta(), 0);
	stats->resetFightStats();
	EXPECT_EQ(stats->getFightStartingTime(), 0);
}

} // namespace
} // namespace aion::gameserver::model::stats::test
