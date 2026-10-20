#include "aion/gameserver/network/aion/clientpackets/CM_BUY_BROKER_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BUY_BROKER_ITEM::CM_BUY_BROKER_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BUY_BROKER_ITEM.java:26-30
void CM_BUY_BROKER_ITEM::readImpl() {
	brokerObjId = readD();
	itemUniqueId = readD();
	itemCount = readQ();
}

// Java CM_BUY_BROKER_ITEM.java:33-41
void CM_BUY_BROKER_ITEM::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (itemCount < 1)
		return;
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().buyBrokerItem(*player, itemUniqueId, itemCount);
	else
		utils::audit::AuditLogger::log(*player, "tried to buy an item from broker without targeting a broker");
}

AION_CLIENT_PACKET(CM_BUY_BROKER_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
