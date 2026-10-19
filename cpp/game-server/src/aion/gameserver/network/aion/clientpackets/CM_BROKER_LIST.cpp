#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_LIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BROKER_LIST::CM_BROKER_LIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BROKER_LIST.java:27-32
void CM_BROKER_LIST::readImpl() {
	brokerObjId = readD();
	sortType = readC(); // 1 - name; 2 - level; 4 - totalPrice; 6 - price for piece
	page = readUH();
	listMask = readUH();
}

// Java CM_BROKER_LIST.java:35-41
void CM_BROKER_LIST::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().showRequestedItems(*player, listMask, sortType, page, nullptr);
	else
		utils::audit::AuditLogger::log(*player, "tried to browse for broker items without targeting a broker");
}

AION_CLIENT_PACKET(CM_BROKER_LIST);

} // namespace aion::gameserver::network::aion::clientpackets
