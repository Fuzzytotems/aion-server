#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_CANCEL.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_EXCHANGE_CANCEL::CM_EXCHANGE_CANCEL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EXCHANGE_CANCEL.java:20-22
void CM_EXCHANGE_CANCEL::readImpl() {
	// 0 bytes
}

// Java CM_EXCHANGE_CANCEL.java:25-28
void CM_EXCHANGE_CANCEL::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::ExchangeService::getInstance().cancelExchange(*activePlayer);
}

AION_CLIENT_PACKET(CM_EXCHANGE_CANCEL);

} // namespace aion::gameserver::network::aion::clientpackets
