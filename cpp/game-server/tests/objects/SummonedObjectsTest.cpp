// P4-11a construction path of SummonedObject and its subclasses, the one the startup run reaches through HousingService.spawnHouses ->
// HouseController.updateSpawns -> VisibleObjectSpawner.spawnHouseNpc (m5a-plan.md W-06): the base initializer looks the NPC template up in
// DataManager.NPC_DATA (Java SummonedObject.java:25, SummonedHouseNpc.java:18), Npc's Objects.requireNonNull turns a missing template into a
// NullPointerException, and setupStatContainers gives the object its game and life stats. Expectations are derived by hand from
// SummonedObject.java, SummonedHouseNpc.java, Npc.java and HouseDecoration.java.
//
// Test doubles (standing in for bodies of later chunks): NPC_DATA and HOUSE_PARTS_DATA are holders bound from XML text, NPC_SKILL_DATA is empty,
// and the creator of the delegation test is an Npc with a CreatureLifeStats part of fixed HP (as CreatureBodiesTest does). SummonedObject itself
// keeps its real stat containers, so the test covers what the startup run runs.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HousePartsData.bind.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/templates/housing/HousePart.h"
#include "aion/gameserver/model/templates/housing/PartType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using runtime::Ptr;
using runtime::Ref;

#define SUMMONED_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

constexpr int32_t BUTLER_NPC_ID = 800100;
constexpr int32_t CREATOR_NPC_ID = 210001;
constexpr int32_t UNKNOWN_NPC_ID = 999999;

const char* const NPC_TEMPLATES = R"(<npc_templates>)"
								  R"(<npc_template npc_id="800100" level="12" name_id="1" name="butler" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="500" maxMp="200"/></npc_template>)"
								  R"(<npc_template npc_id="210001" level="30" name_id="1" name="creator" rank="NOVICE" rating="NORMAL" tribe="GENERAL">)"
								  R"(<stats maxHp="700" maxMp="300"/></npc_template>)"
								  R"(</npc_templates>)";

const char* const HOUSE_PARTS = R"(<house_parts>)"
								R"(<house_part id="1001" name="basic_roof" quality="COMMON" type="ROOF" building_tags="basic"/>)"
								R"(</house_parts>)";

/** Life stats with fixed HP for the creator npc (CreatureBodiesTest uses the same double) */
class FixedLifeStats final : public stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** The creator of the delegation test: an Npc with the real constructor chain and doubled stat containers */
class CreatorNpc final : public Npc {
	AION_MAKE_REF_FRIEND
public:
	CreatorNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~CreatorNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

/**
 * SummonedObject has a protected constructor like every visible object; the test subclass adds nothing, so the real base initializer (template
 * lookup) and the real setupStatContainers run. It stands for SummonedHouseNpc, whose own constructor only adds the master name, the known list
 * and the effect controller (SummonedHouseNpc.java:18-23) and needs a House the object tests cannot build.
 */
class TestSummonedObject final : public SummonedObject {
	AION_MAKE_REF_FRIEND
public:
	TestSummonedObject(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		int8_t level, Ptr<VisibleObject> creator)
		: SummonedObject(key, std::move(controller), spawnTemplate, level, creator) {}

protected:
	~TestSummonedObject() override = default;
};

class SummonedObjectsTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 4));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		xml::LoadContext context;
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES));
		dataholders::DataManager::HOUSE_PARTS_DATA.publish(xml::bindString<dataholders::HousePartsData>(context, HOUSE_PARTS));
		butlerGroup = templates::spawns::SpawnGroup::create(210010000, BUTLER_NPC_ID, 0, nullptr);
		butlerSpawn = templates::spawns::SpawnTemplate::create(*butlerGroup, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0);
		creatorGroup = templates::spawns::SpawnGroup::create(210010000, CREATOR_NPC_ID, 0, nullptr);
		creatorSpawn = templates::spawns::SpawnTemplate::create(*creatorGroup, 11.0f, 21.0f, 31.0f, int8_t{0}, 0, std::nullopt, 0);
		unknownGroup = templates::spawns::SpawnGroup::create(210010000, UNKNOWN_NPC_ID, 0, nullptr);
		unknownSpawn = templates::spawns::SpawnTemplate::create(*unknownGroup, 12.0f, 22.0f, 32.0f, int8_t{0}, 0, std::nullopt, 0);
	}

	void TearDown() override {
		butlerSpawn.reset();
		creatorSpawn.reset();
		unknownSpawn.reset();
		butlerGroup.reset();
		creatorGroup.reset();
		unknownGroup.reset();
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::HOUSE_PARTS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	Ref<TestSummonedObject> createSummoned(templates::spawns::SpawnTemplate& spawn, int8_t level, Ptr<VisibleObject> creator) {
		return VisibleObject::create<TestSummonedObject>(std::make_unique<controllers::NpcController>(), spawn, level, creator);
	}

	runtime::ManualClock clock{0};
	Ref<templates::spawns::SpawnGroup> butlerGroup;
	Ref<templates::spawns::SpawnGroup> creatorGroup;
	Ref<templates::spawns::SpawnGroup> unknownGroup;
	Ref<templates::spawns::SpawnTemplate> butlerSpawn;
	Ref<templates::spawns::SpawnTemplate> creatorSpawn;
	Ref<templates::spawns::SpawnTemplate> unknownSpawn;
};

TEST_F(SummonedObjectsTest, TheBaseInitializerResolvesTheNpcTemplateOfTheSpawn) {
	SUMMONED_TEST_SCOPE;
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, nullptr);
	ASSERT_TRUE(summoned->getObjectTemplate());
	EXPECT_EQ(summoned->getObjectTemplate()->getTemplateId(), BUTLER_NPC_ID) << "DataManager.NPC_DATA.getNpcTemplate(spawnTemplate.getNpcId())";
	EXPECT_EQ(summoned->getObjectTemplate()->getLevel(), 12);
	EXPECT_EQ(summoned->getLevel(), 7) << "SummonedObject.getLevel returns the constructor argument, not the template level";
}

TEST_F(SummonedObjectsTest, SetupStatContainersGivesGameAndLifeStats) {
	SUMMONED_TEST_SCOPE;
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, nullptr);
	ASSERT_TRUE(summoned->getGameStats()) << "Java: setGameStats(new SummonedObjectGameStats(this))";
	EXPECT_TRUE(dynamic_cast<stats::container::SummonedObjectGameStats*>(summoned->getGameStats().rawPointer()))
		<< "the part is the SummonedObjectGameStats of SummonedObject.setupStatContainers, no longer the NpcGameStats stand-in";
	// what that class changes and the base does not: the five combat stats Java lists carry the summoned bonus rate
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getStat(stats::container::StatEnum::PHYSICAL_ATTACK, 100.0f)->getBonusRate(), 0.2f);
	EXPECT_FLOAT_EQ(summoned->getGameStats()->getStat(stats::container::StatEnum::MAXHP, 100.0f)->getBonusRate(), 1.0f);
	ASSERT_TRUE(summoned->getLifeStats()) << "Java: setLifeStats(new NpcLifeStats(this))";
	EXPECT_EQ(summoned->getLifeStats()->getMaxHp(), 500) << "the <stats maxHp> of the npc template";
	EXPECT_EQ(summoned->getLifeStats()->getCurrentHp(), 500) << "NpcLifeStats starts at the maximum";
	EXPECT_FALSE(summoned->getLifeStats()->isDead());
}

TEST_F(SummonedObjectsTest, AMissingNpcTemplateThrowsNullPointerException) {
	SUMMONED_TEST_SCOPE;
	// Java Npc: Objects.requireNonNull(objectTemplate) on the null lookup result
	EXPECT_THROW(static_cast<void>(createSummoned(*unknownSpawn, int8_t{7}, nullptr)), runtime::NullPointerException);
}

TEST_F(SummonedObjectsTest, WithoutACreatorTheObjectIsItsOwnMaster) {
	SUMMONED_TEST_SCOPE;
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, nullptr);
	EXPECT_FALSE(summoned->getCreator());
	EXPECT_EQ(summoned->getMaster().get(), summoned.get()) << "SummonedObject.getMaster: this unless the creator is a Creature";
	EXPECT_FALSE(summoned->getMasterName()) << "Npc's master name, null while unset";
	EXPECT_EQ(summoned->getCreatorId(), 0);
}

TEST_F(SummonedObjectsTest, ACreatureCreatorSuppliesMasterNameCreatorIdAndMaster) {
	SUMMONED_TEST_SCOPE;
	Ref<CreatorNpc> creator = VisibleObject::create<CreatorNpc>(std::make_unique<controllers::NpcController>(), *creatorSpawn,
		dataholders::DataManager::NPC_DATA->getNpcTemplate(CREATOR_NPC_ID));
	Ref<TestSummonedObject> summoned = createSummoned(*butlerSpawn, int8_t{7}, Ptr<VisibleObject>(*creator));
	EXPECT_EQ(summoned->getCreator().get(), static_cast<VisibleObject*>(creator.get()));
	EXPECT_EQ(summoned->getMaster().get(), static_cast<Creature*>(creator.get())) << "the creator is a Creature";
	ASSERT_TRUE(summoned->getMasterName());
	EXPECT_EQ(*summoned->getMasterName(), creator->getName()) << "SummonedObject.getMasterName falls back to the creator's name";
	EXPECT_EQ(summoned->getCreatorId(), creator->getObjectId());
	EXPECT_EQ(summoned->getRace(), creator->getRace()) << "SummonedObject.getRace delegates to a Creature creator";
}

TEST_F(SummonedObjectsTest, HouseDecorationReadsItsPartTemplate) {
	SUMMONED_TEST_SCOPE;
	Ref<HouseDecoration> decoration = HouseDecoration::create(4242, 1001);
	ASSERT_TRUE(decoration->getTemplate()) << "DataManager.HOUSE_PARTS_DATA.getPartById(templateId)";
	EXPECT_EQ(decoration->getTemplate()->getId(), 1001);
	EXPECT_EQ(decoration->getTemplate()->getType(), templates::housing::PartType::ROOF);
	EXPECT_EQ(decoration->getName(), "basic_roof") << "HouseDecoration.getName: getTemplate().getName()";
	EXPECT_FALSE(dataholders::DataManager::HOUSE_PARTS_DATA->getPartById(7)) << "Java: a null template for an unknown part id";
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
