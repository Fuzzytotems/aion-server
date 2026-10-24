#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SETTLE_ACCOUNT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BROKER_SETTLE_ACCOUNT::CM_BROKER_SETTLE_ACCOUNT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BROKER_SETTLE_ACCOUNT.java:24-26
void CM_BROKER_SETTLE_ACCOUNT::readImpl() {
	brokerObjId = readD();
}

// Java CM_BROKER_SETTLE_ACCOUNT.java:29-35
void CM_BROKER_SETTLE_ACCOUNT::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().settleAccount(*player);
	else
		utils::audit::AuditLogger::log(*player, "tried to get Kinah and unsold items from the broker without targeting a broker");
}

AION_CLIENT_PACKET(CM_BROKER_SETTLE_ACCOUNT);

} // namespace aion::gameserver::network::aion::clientpackets
