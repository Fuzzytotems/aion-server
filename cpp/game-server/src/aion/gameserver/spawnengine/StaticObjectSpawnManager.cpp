#include "aion/gameserver/spawnengine/StaticObjectSpawnManager.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::spawnengine {

void StaticObjectSpawnManager::spawnTemplate(model::templates::spawns::SpawnGroup& spawn, int32_t instanceIndex) {
	const model::templates::VisibleObjectTemplate* objectTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(spawn.getNpcId());
	if (objectTemplate == nullptr)
		return;

	// Java: for each pool spot (spawn.hasPool(): resetPoolSpots, then getPool() times reserveRandomFreePoolSpot) or each spawn template:
	// StaticObject staticObject = new StaticObject(new StaticObjectController(), template, objectTemplate);
	// staticObject.setKnownlist(new PlayerAwareKnownList(staticObject)); bringIntoWorld(staticObject, template, instanceIndex);
	// The StaticObject constructor (P4-11a) is declared but not defined yet, so creating one would not link.
	static_cast<void>(instanceIndex);
	AION_UNPORTED();
}

void StaticObjectSpawnManager::bringIntoWorld(model::gameobjects::VisibleObject& visibleObject, model::templates::spawns::SpawnTemplate& spawn,
	int32_t instanceIndex) {
	world::World& world = world::World::getInstance();
	world.storeObject(visibleObject);
	world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(visibleObject), spawn.getWorldId(), instanceIndex, spawn.getX(), spawn.getY(),
		spawn.getZ(), spawn.getHeading());
	world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(visibleObject));
}

} // namespace aion::gameserver::spawnengine
