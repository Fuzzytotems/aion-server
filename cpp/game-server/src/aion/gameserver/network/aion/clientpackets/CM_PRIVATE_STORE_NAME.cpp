#include "aion/gameserver/network/aion/clientpackets/CM_PRIVATE_STORE_NAME.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/PrivateStoreService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PRIVATE_STORE_NAME::CM_PRIVATE_STORE_NAME(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PRIVATE_STORE_NAME.java:27-29
void CM_PRIVATE_STORE_NAME::readImpl() {
	name = readS();
}

// Java CM_PRIVATE_STORE_NAME.java:32-35
void CM_PRIVATE_STORE_NAME::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::PrivateStoreService::openPrivateStore(*activePlayer, name);
}

AION_CLIENT_PACKET(CM_PRIVATE_STORE_NAME);

} // namespace aion::gameserver::network::aion::clientpackets
