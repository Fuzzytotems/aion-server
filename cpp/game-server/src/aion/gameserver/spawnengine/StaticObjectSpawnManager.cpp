#include "aion/gameserver/spawnengine/StaticObjectSpawnManager.h"

#include <cstdint>
#include <memory>

#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::spawnengine {

namespace {

using model::gameobjects::StaticObject;
using model::gameobjects::VisibleObject;
using model::templates::spawns::SpawnTemplate;

} // namespace

void StaticObjectSpawnManager::spawnTemplate(model::templates::spawns::SpawnGroup& spawn, int32_t instanceIndex) {
	const model::templates::VisibleObjectTemplate* objectTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(spawn.getNpcId());
	if (objectTemplate == nullptr)
		return;

	if (spawn.hasPool()) {
		spawn.resetPoolSpots(instanceIndex);
		for (int32_t i = 0; i < spawn.getPool(); i++) {
			runtime::Ptr<SpawnTemplate> template_ = spawn.reserveRandomFreePoolSpot(instanceIndex);
			// Java: the StaticObject constructor dereferences a null spot (NullPointerException when no free spot is left)
			SpawnTemplate& spot = *template_;
			runtime::Ref<StaticObject> staticObject =
				VisibleObject::create<StaticObject>(std::make_unique<controllers::StaticObjectController>(), spot, objectTemplate);
			staticObject->setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*staticObject));
			bringIntoWorld(*staticObject, spot, instanceIndex);
		}
	} else {
		for (const runtime::Ptr<SpawnTemplate>& template_ : spawn.getSpawnTemplates().snapshot()) {
			runtime::Ref<StaticObject> staticObject =
				VisibleObject::create<StaticObject>(std::make_unique<controllers::StaticObjectController>(), *template_, objectTemplate);
			staticObject->setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*staticObject));
			bringIntoWorld(*staticObject, *template_, instanceIndex);
		}
	}
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
