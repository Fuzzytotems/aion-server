#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_REGISTERED.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BROKER_REGISTERED::CM_BROKER_REGISTERED(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BROKER_REGISTERED.java:24-26
void CM_BROKER_REGISTERED::readImpl() {
	brokerObjId = readD();
}

// Java CM_BROKER_REGISTERED.java:29-35
void CM_BROKER_REGISTERED::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().showRegisteredItems(*player);
	else
		utils::audit::AuditLogger::log(*player, "tried to view his registered broker items without targeting a broker");
}

AION_CLIENT_PACKET(CM_BROKER_REGISTERED);

} // namespace aion::gameserver::network::aion::clientpackets
