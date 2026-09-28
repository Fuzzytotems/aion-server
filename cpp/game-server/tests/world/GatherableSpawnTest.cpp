// P4-10 / P4-11b, m5a-plan.md W-04 and W-07: VisibleObjectSpawner.spawnGatherable and the GatherableController lifetime it needs. Until stage 2
// the spawner was an AION_PARTIAL and the world held no Gatherable at all, so no SM_GATHERABLE_INFO was ever sent (scenario check V4, real-client
// checklist item 7). Expectations are derived from Java Gatherable.java (object id from IDFactory, template from DataManager.GATHERABLE_DATA,
// PlayerAwareKnownList), VisibleObjectSpawner.java:139-143 and GatherableController.java (completeInteraction, cancelGathering,
// getGatheringPlayerId with a null task).
//
// Test doubles: the world maps and zones of WorldTestSupport.h, and a GATHERABLE_DATA holder bound from XML text. The Gatherable, its controller
// and its known list are the real ones, so the test exercises what SpawnEngine.spawnAll runs.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>

#include "WorldTestSupport.h"

#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.bind.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::world::test {
namespace {

using model::gameobjects::Gatherable;
using model::gameobjects::VisibleObject;

constexpr int32_t KUKURU_ID = 400001;
constexpr int32_t UNKNOWN_GATHERABLE_ID = 499999;

/** the real attribute set of gatherable_templates.xml (the first entry of the shipped file) */
const char* const GATHERABLE_TEMPLATES =
	R"(<gatherable_templates>)"
	R"(<gatherable_template id="400001" name="Kukuru" nameId="701957" sourceType="VEGETABLE" harvestCount="3" skillLevel="20" harvestSkill="30002" successAdj="100" failureAdj="100" aerialAdj="100">)"
	R"(<materials><material rate="10000000" nameid="702021" itemid="152000001" name="Kukuru"/></materials>)"
	R"(</gatherable_template>)"
	R"(</gatherable_templates>)";

class GatherableSpawnTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		if (!dataholders::DataManager::GATHERABLE_DATA) {
			xml::LoadContext context;
			dataholders::DataManager::GATHERABLE_DATA.publish(xml::bindString<dataholders::GatherableData>(context, GATHERABLE_TEMPLATES));
		}
	}

	static runtime::Ref<model::templates::spawns::SpawnTemplate> spawnOf(int32_t gatherableId, float x, float y) {
		group = model::templates::spawns::SpawnGroup::create(POETA, gatherableId, 0, nullptr);
		return model::templates::spawns::SpawnTemplate::create(*group, x, y, 10.0f, int8_t{0}, 0, std::nullopt, 0);
	}

	inline static runtime::Ref<model::templates::spawns::SpawnGroup> group;
};

TEST_F(GatherableSpawnTest, SpawnGatherableBringsARealGatherableIntoTheWorld) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	World& world = World::getInstance();
	runtime::Ref<model::templates::spawns::SpawnTemplate> spawn = spawnOf(KUKURU_ID, 420, 420);
	const uint64_t unportedBefore = runtime::unportedHitCount();

	runtime::Ref<Gatherable> gatherable = spawnengine::VisibleObjectSpawner::spawnGatherable(*spawn, 1);
	ASSERT_TRUE(gatherable) << "Java returns the new Gatherable, never null";

	EXPECT_TRUE(gatherable->isSpawned());
	EXPECT_TRUE(world.isInWorld(gatherable->getObjectId()));
	EXPECT_NE(gatherable->getObjectId(), 0) << "the object id comes from IDFactory";
	ASSERT_NE(gatherable->getObjectTemplate(), nullptr) << "DataManager.GATHERABLE_DATA.getGatherableTemplate(spawn.getNpcId())";
	EXPECT_EQ(gatherable->getObjectTemplate()->getTemplateId(), KUKURU_ID);
	EXPECT_EQ(gatherable->getObjectTemplate()->getHarvestCount(), 3);
	EXPECT_EQ(gatherable->getSpawn().get(), spawn.get());
	EXPECT_FLOAT_EQ(gatherable->getX(), 420.0f);
	EXPECT_FLOAT_EQ(gatherable->getY(), 420.0f);
	EXPECT_EQ(gatherable->getPosition()->getMapId(), POETA);
	// Java: setKnownlist(new PlayerAwareKnownList(this))
	EXPECT_NE(dynamic_cast<knownlist::PlayerAwareKnownList*>(&gatherable->getKnownList()), nullptr);
	// Java: controller.setOwner(this), and VisibleObjectController<Gatherable>.getOwner() narrows
	EXPECT_EQ(&gatherable->getController().getOwner(), gatherable.get());

	// no gathering is in progress: the Java body returns 0 without touching the task
	EXPECT_EQ(gatherable->getController().getGatheringPlayerId(), 0);

	// onDespawn -> cancelGathering with a null task returns silently (it threw while the task member had no complete type)
	EXPECT_TRUE(gatherable->getController().delete_());
	EXPECT_FALSE(gatherable->isSpawned());
	EXPECT_FALSE(world.isInWorld(gatherable->getObjectId()));
	EXPECT_EQ(runtime::unportedHitCount(), unportedBefore) << "the spawn and despawn path of a gatherable reaches no unported body";
}

TEST_F(GatherableSpawnTest, AGatherableWithoutATemplateIsStillSpawned) {
	// Java: Gatherable's super(...) takes the null template without complaining (VisibleObject.objectTemplate is nullable); only
	// getObjectTemplate() calls on it would fail later. The spawner has no template check, unlike spawnNpc.
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<model::templates::spawns::SpawnTemplate> spawn = spawnOf(UNKNOWN_GATHERABLE_ID, 460, 460);

	runtime::Ref<Gatherable> gatherable = spawnengine::VisibleObjectSpawner::spawnGatherable(*spawn, 1);
	ASSERT_TRUE(gatherable);
	EXPECT_EQ(gatherable->getObjectTemplate(), nullptr);
	EXPECT_TRUE(gatherable->isSpawned());

	EXPECT_TRUE(gatherable->getController().delete_());
}

} // namespace
} // namespace aion::gameserver::world::test
