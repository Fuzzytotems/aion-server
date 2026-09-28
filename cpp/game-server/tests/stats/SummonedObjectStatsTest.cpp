// P5-01 SummonedObjectGameStats (wave 5a stage 2, item X-01a; header request hr-1 of the world-visibility lane): the stat container Java's
// SummonedObject.setupStatContainers installs. Expectations follow SummonedObjectGameStats.java and SummonedObject.getMaster().
//
// Test double: SummonedObject's constructor is protected like every visible object's, so a test subclass adds nothing but the Java
// setupStatContainers body, which SummonedObject itself now runs as well (P4-11a). NPC_DATA is bound from XML text.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
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

constexpr int32_t MASTER_NPC_ID = 700102;
constexpr int32_t SUMMONED_NPC_ID = 700103;

const char* const NPC_TEMPLATES =
	R"(<npc_templates>)"
	R"(<npc_template npc_id="700102" level="10" name_id="1" name="master" rank="NOVICE" rating="NORMAL" tribe="GENERAL" attack_speed="2000">)"
	R"(<stats maxHp="1000" maxMp="100" attack="20" mboost="250"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
	R"(<npc_template npc_id="700103" level="4" name_id="1" name="summoned" rank="NOVICE" rating="NORMAL" tribe="GENERAL" attack_speed="2000">)"
	R"(<stats maxHp="500" maxMp="100" attack="16" mboost="0"><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
	R"(</npc_templates>)";

/** A spawn template of the group, like the spawn data of a map */
class SummonedSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit SummonedSpawnTemplate(templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Adds nothing but Java's SummonedObject.setupStatContainers, which the C++ P4-11a body cannot run until this class exists (hr-1) */
class TestSummonedObject final : public gameobjects::SummonedObject {
	AION_MAKE_REF_FRIEND
public:
	TestSummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller,
		templates::spawns::SpawnTemplate& spawnTemplate, int8_t level, Ptr<gameobjects::VisibleObject> creator)
		: SummonedObject(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestSummonedObject() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<container::SummonedObjectGameStats>(*this));
		setLifeStats(std::make_unique<container::NpcLifeStats>(*this));
	}
};

class SummonedObjectStatsTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 31));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, NPC_TEMPLATES));
	}

	void TearDown() override {
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		groups.clear();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	templates::spawns::SpawnTemplate& makeSpawn(int32_t npcId) {
		Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		templates::spawns::SpawnTemplate& spawnTemplate = group->addSpawnTemplate(std::make_unique<SummonedSpawnTemplate>(*group));
		groups.push_back(group);
		return spawnTemplate;
	}

	Ref<TestSummonedObject> makeSummoned(int8_t level, Ptr<gameobjects::VisibleObject> creator) {
		Ref<TestSummonedObject> summoned = gameobjects::VisibleObject::create<TestSummonedObject>(
			std::make_unique<controllers::NpcController>(), makeSpawn(SUMMONED_NPC_ID), level, creator);
		summoned->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*summoned));
		return summoned;
	}

	runtime::ManualClock clock{0};
	xml::LoadContext npcContext;
	std::vector<Ref<templates::spawns::SpawnGroup>> groups;
};

TEST_F(SummonedObjectStatsTest, WithoutACreatureMasterOnlyTheBonusRatesAreSet) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Java SummonedObject.getMaster(): a creator that is no Creature (a house) leaves the object its own master
	Ref<TestSummonedObject> summoned = makeSummoned(int8_t{5}, nullptr);
	EXPECT_EQ(summoned->getMaster().rawPointer(), static_cast<gameobjects::Creature*>(summoned.get()));
	EXPECT_EQ(summoned->getLevel(), 5) << "SummonedObject.getLevel() is the constructor argument";

	Ptr<container::NpcGameStats> stats = summoned->getGameStats();
	ASSERT_TRUE(stats);
	for (StatEnum boosted : {StatEnum::MAGICAL_ATTACK, StatEnum::MAGICAL_ACCURACY, StatEnum::MAGICAL_RESIST, StatEnum::PHYSICAL_ACCURACY,
			 StatEnum::PHYSICAL_ATTACK})
		EXPECT_FLOAT_EQ(stats->getStat(boosted, 100.0f)->getBonusRate(), 0.2f) << "Java: stat.setBonusRate(0.2f)";
	EXPECT_FLOAT_EQ(stats->getStat(StatEnum::MAXHP, 100.0f)->getBonusRate(), 1.0f) << "a stat outside the switch is untouched";

	// the NpcGameStats behaviour is unchanged (the stats template of NPC_DATA, not of the master)
	EXPECT_EQ(stats->getMaxHp()->getCurrent(), 500);
	EXPECT_EQ(stats->getBaseAttackSpeed(), 2000);
	EXPECT_TRUE(summoned->getLifeStats());
}

TEST_F(SummonedObjectStatsTest, WithACreatureMasterTheMagicBoostIsSixtyPercentOfTheMasterBoost) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<gameobjects::Npc> master = gameobjects::VisibleObject::create<gameobjects::Npc>(std::make_unique<controllers::NpcController>(),
		makeSpawn(MASTER_NPC_ID), dataholders::DataManager::NPC_DATA->getNpcTemplate(MASTER_NPC_ID));
	master->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*master));
	EXPECT_EQ(master->getGameStats()->getMBoost()->getCurrent(), 250) << "the master's own magic boost";

	Ref<TestSummonedObject> summoned = makeSummoned(int8_t{5}, Ptr<gameobjects::VisibleObject>(*master));
	EXPECT_EQ(summoned->getMaster().rawPointer(), static_cast<gameobjects::Creature*>(master.get()))
		<< "SummonedObject.getMaster(): the creator when it is a Creature";

	// Java: getStat(BOOST_MAGICAL_SKILL, (int) (owner.getMaster().getGameStats().getMBoost().getCurrent() * 0.6f))
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getMBoost()->getExactBaseWithoutBaseRate(), 150.0f);
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getStat(StatEnum::MAGICAL_ATTACK, 100.0f)->getBonusRate(), 0.2f);
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getStat(StatEnum::MAXHP, 100.0f)->getBonusRate(), 1.0f);
}

TEST_F(SummonedObjectStatsTest, ASelfMasterReadsItsOwnMagicBoostInsteadOfRecursing) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Java: owner.getMaster() is the object itself for a creator that is no Creature (a house npc), so getMBoost() recurses until the stack is
	// exhausted (StackOverflowError). The same recursion would take this process down, so the self-master case falls back to
	// CreatureGameStats.getMBoost, i.e. the npc's own stats template (docs/deviations/P5-01.md).
	Ref<TestSummonedObject> summoned = makeSummoned(int8_t{5}, nullptr);
	ASSERT_EQ(summoned->getMaster().rawPointer(), static_cast<gameobjects::Creature*>(summoned.get()));
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getMBoost()->getExactBaseWithoutBaseRate(), 0.0f) << "<stats mboost=\"0\"> of npc 700103";
	EXPECT_EQ(summoned->getGameStats()->getMBoost()->getCurrent(), 0);
}

} // namespace
} // namespace aion::gameserver::model::stats::test
