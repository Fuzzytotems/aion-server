// Static objects of the spawn managers (P4-11a, m5a-plan.md W-04): StaticObject and StaticDoor construction through VisibleObject::create<T> with
// the StaticObjectController part. Expectations are hand-derived from StaticObject.java, StaticDoor.java and StaticDoorState.java (flags NONE 0,
// OPENED 1, CLICKABLE 2, ...).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>

#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorState.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.bind.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using templates::staticdoor::StaticDoorState;

class StaticObjectsTestSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit StaticObjectsTestSpawnTemplate(templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Door templates are immortal static data: kept for the process like the holder keeps them */
const templates::staticdoor::StaticDoorTemplate* doorTemplate(const char* attributes) {
	xml::LoadContext context;
	return xml::bindString<templates::staticdoor::StaticDoorTemplate>(context, std::string("<staticdoor ") + attributes + "/>").release();
}

class StaticObjectsTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		group = templates::spawns::SpawnGroup::create(210010000, 300001, 0, nullptr);
		spawnTemplate = runtime::Ref<StaticObjectsTestSpawnTemplate>(
			static_cast<StaticObjectsTestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<StaticObjectsTestSpawnTemplate>(*group))));
	}

	void TearDown() override {
		spawnTemplate.reset();
		group.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::idfactory::IDFactory::getInstance().resetForTests();
	}

	runtime::Ref<templates::spawns::SpawnGroup> group;
	runtime::Ref<StaticObjectsTestSpawnTemplate> spawnTemplate;
};

TEST_F(StaticObjectsTest, StaticObjectTakesAnIdAPositionOfItsWorldAndBindsItsController) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<StaticObject> first = VisibleObject::create<StaticObject>(std::make_unique<controllers::StaticObjectController>(), *spawnTemplate, nullptr);
	runtime::Ref<StaticObject> second = VisibleObject::create<StaticObject>(std::make_unique<controllers::StaticObjectController>(), *spawnTemplate, nullptr);
	EXPECT_NE(first->getObjectId(), second->getObjectId()) << "IDFactory.nextId()";
	ASSERT_TRUE(first->getPosition());
	EXPECT_EQ(first->getPosition()->getMapId(), 210010000) << "new WorldPosition(spawnTemplate.getWorldId())";
	EXPECT_EQ(first->getSpawn().get(), static_cast<templates::spawns::SpawnTemplate*>(spawnTemplate.get()));
	auto& controller = static_cast<controllers::StaticObjectController&>(first->getController());
	EXPECT_EQ(&controller.getOwner(), first.get()) << "controller.setOwner(this)";
}

TEST_F(StaticObjectsTest, StaticDoorStatesAndLockFollowTheTemplate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const templates::staticdoor::StaticDoorTemplate* openLocked = doorTemplate(R"(id="7" keyid="185000001" state="3" x="1" y="2" z="3")");
	const templates::staticdoor::StaticDoorTemplate* closedFree = doorTemplate(R"(id="8" keyid="1" state="2")");

	runtime::Ref<StaticDoor> open =
		VisibleObject::create<StaticDoor>(std::make_unique<controllers::StaticObjectController>(), *spawnTemplate, openLocked, 1);
	EXPECT_TRUE(open->isOpen());
	EXPECT_EQ(open->getStates(), (std::set<StaticDoorState>{StaticDoorState::OPENED, StaticDoorState::CLICKABLE}));
	EXPECT_TRUE(open->isLocked()) << "key id >= 2";
	EXPECT_EQ(open->getObjectTemplate(), openLocked);

	runtime::Ref<StaticDoor> closed =
		VisibleObject::create<StaticDoor>(std::make_unique<controllers::StaticObjectController>(), *spawnTemplate, closedFree, 1);
	EXPECT_FALSE(closed->isOpen());
	EXPECT_EQ(closed->getStates(), (std::set<StaticDoorState>{StaticDoorState::CLICKABLE}));
	EXPECT_FALSE(closed->isLocked()) << "key id < 2";

	EXPECT_THROW(static_cast<void>(VisibleObject::create<StaticDoor>(std::make_unique<controllers::StaticObjectController>(), *spawnTemplate,
					 static_cast<const templates::staticdoor::StaticDoorTemplate*>(nullptr), 1)),
		runtime::NullPointerException)
		<< "Java: getObjectTemplate().getState() on a null template";
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
