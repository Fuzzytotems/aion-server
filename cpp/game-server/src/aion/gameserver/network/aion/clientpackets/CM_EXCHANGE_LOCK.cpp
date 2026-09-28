#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_LOCK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_EXCHANGE_LOCK::CM_EXCHANGE_LOCK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EXCHANGE_LOCK.java:20-22
void CM_EXCHANGE_LOCK::readImpl() {
	// nothing
}

// Java CM_EXCHANGE_LOCK.java:25-28
void CM_EXCHANGE_LOCK::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::ExchangeService::getInstance().lockExchange(*activePlayer);
}

AION_CLIENT_PACKET(CM_EXCHANGE_LOCK);

} // namespace aion::gameserver::network::aion::clientpackets
