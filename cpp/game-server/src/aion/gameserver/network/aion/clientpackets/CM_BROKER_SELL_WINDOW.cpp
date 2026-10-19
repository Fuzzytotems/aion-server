#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SELL_WINDOW.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/BrokerService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BROKER_SELL_WINDOW::CM_BROKER_SELL_WINDOW(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BROKER_SELL_WINDOW.java:22-24
void CM_BROKER_SELL_WINDOW::readImpl() {
	this->itemUniqueId = readD();
}

// Java CM_BROKER_SELL_WINDOW.java:27-34
void CM_BROKER_SELL_WINDOW::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTrading())
		return;
	services::BrokerService::getInstance().showSellWindow(*player, itemUniqueId);
}

AION_CLIENT_PACKET(CM_BROKER_SELL_WINDOW);

} // namespace aion::gameserver::network::aion::clientpackets
