#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SETTLE_LIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BROKER_SETTLE_LIST::CM_BROKER_SETTLE_LIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BROKER_SETTLE_LIST.java:24-27
void CM_BROKER_SETTLE_LIST::readImpl() {
	brokerObjId = readD();
	startPageIndex = readUH();
}

// Java CM_BROKER_SETTLE_LIST.java:30-36
void CM_BROKER_SETTLE_LIST::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().showSettledItems(*player, startPageIndex);
	else
		utils::audit::AuditLogger::log(*player, "tried to open the broker sold item list without targeting a broker");
}

AION_CLIENT_PACKET(CM_BROKER_SETTLE_LIST);

} // namespace aion::gameserver::network::aion::clientpackets
