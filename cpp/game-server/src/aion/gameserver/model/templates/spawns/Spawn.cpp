// Spawn's beforeMarshal and afterMarshal are replaced: SpawnsData's writer (saveSpawn, D14) leaves the attributes out that the
// hooks set to null (pool 0 and custom false) without changing the fields, so there is nothing to restore after marshalling.

#include "aion/gameserver/model/templates/spawns/Spawn.h"

namespace aion::gameserver::model::templates::spawns {

Spawn::Spawn(int32_t npcIdValue, int32_t respawnTimeValue, std::optional<spawnengine::SpawnHandlerType> handlerValue)
	: npcId(npcIdValue), respawnTime(respawnTimeValue), handler(handlerValue) {
}

} // namespace aion::gameserver::model::templates::spawns
