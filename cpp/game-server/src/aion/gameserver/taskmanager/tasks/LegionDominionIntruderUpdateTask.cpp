#include "aion/gameserver/taskmanager/tasks/LegionDominionIntruderUpdateTask.h"

#include <map>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/templates/cp/CPType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::taskmanager::tasks {

using model::gameobjects::player::Player;
using model::legionDominion::LegionDominionLocation;

// Java LegionDominionIntruderUpdateTask.java:24-26
LegionDominionIntruderUpdateTask::LegionDominionIntruderUpdateTask() : AbstractPeriodicTaskManager(4000, "LegionDominionIntruderUpdateTask") {
}

LegionDominionIntruderUpdateTask::~LegionDominionIntruderUpdateTask() = default;

LegionDominionIntruderUpdateTask& LegionDominionIntruderUpdateTask::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<LegionDominionIntruderUpdateTask>& instance =
		*new runtime::Ref<LegionDominionIntruderUpdateTask>(runtime::makeRef<LegionDominionIntruderUpdateTask>());
	return *instance;
}

// Java LegionDominionIntruderUpdateTask.java:28-64
void LegionDominionIntruderUpdateTask::run() {
	if (!configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load())
		return;
	// Java: Collectors.groupingBy into a HashMap; C++: ordered by world id (the order of the worlds is not observable per player)
	std::map<int32_t, std::vector<runtime::Ptr<LegionDominionLocation>>> locationsPerWorldMap;
	for (const runtime::Ptr<LegionDominionLocation>& loc : services::LegionDominionService::getInstance().getLegionDominions())
		if (loc->getLegionId() != 0)
			locationsPerWorldMap[loc->getWorldId()].push_back(loc);
	if (locationsPerWorldMap.empty())
		return;

	for (const auto& [worldId, locations] : locationsPerWorldMap) {
		const std::vector<runtime::Ptr<Player>> players = world::World::getInstance().getWorldMap(worldId)->getMainWorldMapInstance()->getPlayersInside();
		for (const runtime::Ptr<LegionDominionLocation>& location : locations) {
			if (location->getZoneNameAsString().empty())
				continue;
			const world::zone::ZoneName* zoneName = world::zone::ZoneName::get(location->getZoneNameAsString());
			if (zoneName == world::zone::ZoneName::NONE)
				continue;
			services::conquerorAndProtectorSystem::ConquerorAndProtectorService& cpService =
				services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance();
			std::vector<runtime::Ptr<Player>> protectors;
			std::vector<runtime::Ptr<Player>> conquerors;
			for (const runtime::Ptr<Player>& player : players) {
				if (!player->isInsideZone(zoneName))
					continue;
				runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> cpInfo = cpService.getCPInfoForCurrentMap(*player);
				if (!cpInfo || (cpInfo->getType() == model::templates::cp::CPType::PROTECTOR && cpInfo->getLDRank() != 3))
					continue;
				if (cpInfo->getType() == model::templates::cp::CPType::CONQUEROR)
					conquerors.push_back(player);
				else if (cpInfo->getType() == model::templates::cp::CPType::PROTECTOR)
					protectors.push_back(player);
			}
			if (!conquerors.empty())
				for (const runtime::Ptr<Player>& player : protectors)
					utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(conquerors, false));
		}
	}
}

} // namespace aion::gameserver::taskmanager::tasks
