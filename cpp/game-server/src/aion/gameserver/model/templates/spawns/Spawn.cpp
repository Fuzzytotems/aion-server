#include "aion/gameserver/model/templates/spawns/Spawn.h"

namespace aion::gameserver::model::templates::spawns {

Spawn::Spawn(int32_t npcIdValue, int32_t respawnTimeValue, std::optional<spawnengine::SpawnHandlerType> handlerValue)
	: npcId(npcIdValue), respawnTime(respawnTimeValue), handler(handlerValue) {
}

} // namespace aion::gameserver::model::templates::spawns
