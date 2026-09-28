#include "aion/gameserver/custom/pvpmap/PvpMapService.h"

#include <optional>
#include <string>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/custom/pvpmap/PvpMapHandler.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::custom::pvpmap {

PvpMapService::PvpMapService() = default;

PvpMapService::~PvpMapService() = default;

PvpMapService& PvpMapService::getInstance() {
	static PvpMapService instance; // Java: private static final PvpMapService instance
	return instance;
}

void PvpMapService::init() {
	// Java:
	//   WorldMapInstance instance = InstanceService.getNextAvailableInstance(301220000, 0, (byte) 0, PvpMapHandler::new, 0, false);
	//   handler = (PvpMapHandler) instance.getInstanceHandler();
	AION_PARTIAL("the PvP map instance is not created: InstanceService.getNextAvailableInstance is not ported (M5a)");
}

void PvpMapService::onLogin(model::gameobjects::player::Player& player) {
	runtime::Ptr<PvpMapHandler> mapHandler = handler.get();
	if (mapHandler && configs::main::CustomConfig::PVP_MAP_ENABLED.load() && mapHandler->isRandomBossAlive())
		notifyBossSpawn(player);
}

void PvpMapService::notifyBossSpawn(model::gameobjects::player::Player& player) {
	bool isOnPvpMap = isOnPvPMap(player);
	if (!isOnPvpMap && player.isInInstance()) // don't notify players inside instances
		return;
	bool isLvSixtyOrHigher = player.getLevel() >= 60;
	if (isOnPvpMap || isLvSixtyOrHigher)
		utils::PacketSendUtility::sendMessage(player, "[PvP-Map] A powerful monster appeared.", model::ChatType::BRIGHT_YELLOW_CENTER);
	if (!isOnPvpMap && isLvSixtyOrHigher)
		utils::PacketSendUtility::sendMessage(player, "You can join the map via " + utils::ChatUtil::color(".pvp join", utils::JavaColor::WHITE),
			model::ChatType::BRIGHT_YELLOW);
}

bool PvpMapService::isRandomBoss(model::gameobjects::Npc& npc) {
	runtime::Ptr<PvpMapHandler> mapHandler = handler.get();
	return mapHandler && mapHandler->isRandomBoss(npc.getObjectId());
}

void PvpMapService::joinMap(model::gameobjects::player::Player& p) {
	runtime::Ptr<PvpMapHandler> mapHandler = handler.get();
	if (mapHandler && !mapHandler->isOnMap(p))
		mapHandler->join(p);
}

void PvpMapService::leaveMap(model::gameobjects::player::Player& p) {
	runtime::Ptr<PvpMapHandler> mapHandler = handler.get();
	if (mapHandler && mapHandler->isOnMap(p))
		mapHandler->leave(p);
}

bool PvpMapService::isOnPvPMap(model::gameobjects::Creature& creature) {
	runtime::Ptr<PvpMapHandler> mapHandler = handler.get();
	return mapHandler && mapHandler->isOnMap(creature);
}

int32_t PvpMapService::getParticipantsSize() {
	runtime::Ptr<PvpMapHandler> mapHandler = handler.get();
	return !mapHandler ? 0 : mapHandler->getParticipantsSize();
}

void PvpMapService::onInstanceDestroy() {
	handler.set(nullptr);
}

} // namespace aion::gameserver::custom::pvpmap
