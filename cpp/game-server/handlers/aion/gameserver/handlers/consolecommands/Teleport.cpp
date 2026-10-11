#include "aion/gameserver/handlers/consolecommands/Teleport.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Teleport);

// Java Teleport.java:22-26
Teleport::Teleport()
	: ConsoleCommand("teleport", "Moves you to any location.",
		  "[mapCName] <x> <y> <z> - Moves you to the specified coordinates on the given map (default: current map).\n") {
}

// Java Teleport.java:28-46
void Teleport::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 3) {
		sendInfo(player);
		return;
	}
	size_t i = 0;
	int32_t worldId = params.size() == 3 ? player.getWorldId() : DataManager::WORLD_MAPS_DATA->getWorldIdByCName(params[i++]);
	if (worldId != 0) {
		int32_t x = commons::utils::parseInt(params[i++]);
		int32_t y = commons::utils::parseInt(params[i++]);
		int32_t inputZ = commons::utils::parseInt(params[i++]);
		float z = static_cast<float>(inputZ);
		runtime::Ptr<WorldMapInstance> instance = getOrCreateWorldMapInstance(player, worldId);
		if (inputZ == 10000) // default value in client when selecting a point on the world map
			z = GeoService::getInstance().getZ(worldId, static_cast<float>(x), static_cast<float>(y), 4000.0f, 0.0f, instance->getInstanceId());
		TeleportService::teleportTo(player, *instance, static_cast<float>(x), static_cast<float>(y), z);
	}
}

// Java Teleport.java:48-55
runtime::Ptr<WorldMapInstance> Teleport::getOrCreateWorldMapInstance(Player& player, int32_t worldId) {
	if (player.getWorldId() == worldId)
		return player.getPosition()->getWorldMapInstance();
	else if (World::getInstance().getWorldMap(worldId)->isInstanceType())
		return InstanceService::getOrRegisterInstance(worldId, player);
	else
		return World::getInstance().getWorldMap(worldId)->getMainWorldMapInstance();
}

} // namespace aion::gameserver::handlers::consolecommands
