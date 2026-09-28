#include "aion/gameserver/spawnengine/StaticDoorSpawnManager.h"

#include <cstdint>
#include <memory>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/StaticDoorData.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.StaticDoorSpawnManager");

void StaticDoorSpawnManager::spawnTemplate(world::WorldMapInstance& instance) {
	int32_t counter = 0;
	for (const model::templates::staticdoor::StaticDoorTemplate* data : dataholders::DataManager::STATICDOOR_DATA->getStaticDoors(instance.getMapId())) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> spawn =
			SpawnEngine::newSingleTimeSpawn(instance.getMapId(), 300001, data->getX(), data->getY(), data->getZ(), int8_t{0});
		spawn->setStaticId(data->getId());
		runtime::Ref<model::gameobjects::StaticDoor> staticDoor = model::gameobjects::VisibleObject::create<model::gameobjects::StaticDoor>(
			std::make_unique<controllers::StaticObjectController>(), *spawn, data, instance.getInstanceId());
		staticDoor->setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*staticDoor));
		SpawnEngine::bringIntoWorld(*staticDoor, *spawn, instance.getInstanceId());
		counter++;
		world::geo::GeoService::getInstance().setDoorState(instance.getMapId(), instance.getInstanceId(), data->getId(), staticDoor->isOpen());
	}
	if (counter > 0)
		log.info("Spawned " + std::to_string(counter) + " static doors in " + instance.toString());
}

} // namespace aion::gameserver::spawnengine
