#include "aion/gameserver/model/templates/spawns/SpawnMap.h"

namespace aion::gameserver::model::templates::spawns {

SpawnMap::SpawnMap(int32_t mapIdValue) : mapId(mapIdValue) {
	// Java: this.spawns = new ArrayList<>() (the C++ vector always exists)
}

} // namespace aion::gameserver::model::templates::spawns
