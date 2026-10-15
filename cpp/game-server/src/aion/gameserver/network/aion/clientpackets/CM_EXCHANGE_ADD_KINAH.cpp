#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_ADD_KINAH.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_EXCHANGE_ADD_KINAH::CM_EXCHANGE_ADD_KINAH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EXCHANGE_ADD_KINAH.java:21-23
void CM_EXCHANGE_ADD_KINAH::readImpl() {
	kinahCount = readQ();
}

// Java CM_EXCHANGE_ADD_KINAH.java:26-28
void CM_EXCHANGE_ADD_KINAH::runImpl() {
	services::ExchangeService::getInstance().addKinah(*getConnection()->getActivePlayer(), kinahCount);
}

AION_CLIENT_PACKET(CM_EXCHANGE_ADD_KINAH);

} // namespace aion::gameserver::network::aion::clientpackets
