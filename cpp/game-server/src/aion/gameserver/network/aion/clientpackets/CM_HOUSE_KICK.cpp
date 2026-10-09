#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_KICK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_HOUSE_KICK::CM_HOUSE_KICK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_KICK.java:25-28
void CM_HOUSE_KICK::readImpl() {
	option = readC();
	readH();
}

// Java CM_HOUSE_KICK.java:31-44
void CM_HOUSE_KICK::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	const runtime::Ptr<model::house::House> house = player->getActiveHouse();
	if (!house) {
		utils::audit::AuditLogger::log(*player, "tried to kick players from house without owning one");
		return;
	}
	if (option == 1)
		house->getController().kickVisitors(player, false, false);
	else if (option == 2)
		house->getController().kickVisitors(player, true, false);
}

AION_CLIENT_PACKET(CM_HOUSE_KICK);

} // namespace aion::gameserver::network::aion::clientpackets
