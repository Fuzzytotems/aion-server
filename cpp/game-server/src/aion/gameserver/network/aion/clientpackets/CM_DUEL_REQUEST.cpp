#include "aion/gameserver/network/aion/clientpackets/CM_DUEL_REQUEST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_DUEL_REQUEST::CM_DUEL_REQUEST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_DUEL_REQUEST.java:30-32
void CM_DUEL_REQUEST::readImpl() {
	objectId = readD();
}

// Java CM_DUEL_REQUEST.java:35-38
void CM_DUEL_REQUEST::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::DuelService::getInstance().onDuelRequest(*activePlayer, activePlayer->getKnownList().getPlayer(objectId));
}

AION_CLIENT_PACKET(CM_DUEL_REQUEST);

} // namespace aion::gameserver::network::aion::clientpackets
