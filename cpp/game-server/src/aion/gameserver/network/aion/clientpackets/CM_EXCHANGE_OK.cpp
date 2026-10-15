#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_OK.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_EXCHANGE_OK::CM_EXCHANGE_OK(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EXCHANGE_OK.java:20-22
void CM_EXCHANGE_OK::readImpl() {
}

// Java CM_EXCHANGE_OK.java:25-28
void CM_EXCHANGE_OK::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::ExchangeService::getInstance().confirmExchange(activePlayer);
}

AION_CLIENT_PACKET(CM_EXCHANGE_OK);

} // namespace aion::gameserver::network::aion::clientpackets
