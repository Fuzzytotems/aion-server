#include "aion/gameserver/network/aion/clientpackets/CM_REGISTER_BROKER_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_REGISTER_BROKER_ITEM::CM_REGISTER_BROKER_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_REGISTER_BROKER_ITEM.java:28-34
void CM_REGISTER_BROKER_ITEM::readImpl() {
	brokerObjId = readD();
	itemUniqueId = readD();
	price = readQ();
	itemCount = readQ();
	splittingAvailable = readC() == 1;
}

// Java CM_REGISTER_BROKER_ITEM.java:37-47
void CM_REGISTER_BROKER_ITEM::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTrading() || itemCount <= 0)
		return;
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().registerItem(*player, itemUniqueId, itemCount, price, splittingAvailable);
	else
		utils::audit::AuditLogger::log(*player, "tried to register a broker item without targeting a broker");
}

AION_CLIENT_PACKET(CM_REGISTER_BROKER_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
