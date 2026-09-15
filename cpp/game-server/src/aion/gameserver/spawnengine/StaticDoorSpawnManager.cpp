#include "aion/gameserver/spawnengine/StaticDoorSpawnManager.h"

#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/StaticDoorData.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::spawnengine {

void StaticDoorSpawnManager::spawnTemplate(world::WorldMapInstance& instance) {
	const std::vector<const model::templates::staticdoor::StaticDoorTemplate*>& doors =
		dataholders::DataManager::STATICDOOR_DATA->getStaticDoors(instance.getMapId());
	if (doors.empty())
		return; // Java: no iteration, counter stays 0 (no log)
	// Java, for each door data:
	//   SpawnTemplate spawn = SpawnEngine.newSingleTimeSpawn(instance.getMapId(), 300001, data.getX(), data.getY(), data.getZ(), (byte) 0);
	//   spawn.setStaticId(data.getId());
	//   StaticDoor staticDoor = new StaticDoor(new StaticObjectController(), spawn, data, instance.getInstanceId());
	//   staticDoor.setKnownlist(new PlayerAwareKnownList(staticDoor)); SpawnEngine.bringIntoWorld(staticDoor, spawn, instance.getInstanceId());
	//   counter++; GeoService.getInstance().setDoorState(mapId, instanceId, data.getId(), staticDoor.isOpen());
	// then: if (counter > 0) log.info("Spawned " + counter + " static doors in " + instance);
	// The StaticDoor constructor (P4-11a) is declared but not defined yet, so creating one would not link.
	AION_UNPORTED();
}

} // namespace aion::gameserver::spawnengine
