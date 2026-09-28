#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_ADD_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_EXCHANGE_ADD_ITEM::CM_EXCHANGE_ADD_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EXCHANGE_ADD_ITEM.java:23-26
void CM_EXCHANGE_ADD_ITEM::readImpl() {
	itemObjId = readD();
	itemCount = readD();
}

// Java CM_EXCHANGE_ADD_ITEM.java:29-32
void CM_EXCHANGE_ADD_ITEM::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::ExchangeService::getInstance().addItem(*activePlayer, itemObjId, itemCount);
}

AION_CLIENT_PACKET(CM_EXCHANGE_ADD_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
