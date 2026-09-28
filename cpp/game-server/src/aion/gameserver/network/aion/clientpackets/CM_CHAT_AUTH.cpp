#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_AUTH.h"

#include <memory>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHAT_AUTH::CM_CHAT_AUTH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CHAT_AUTH::readImpl() {
	objectId = readD(); // lol NC
	macAddress = readB(6);
}

void CM_CHAT_AUTH::runImpl() {
	// C++: sendPlayerLoginRequest takes Player&, so the active player is dereferenced here. IN_GAME (the only valid state) implies a non-null
	// active player (AionConnection::setActivePlayer(null) drops back to AUTHED), so Java's null case is unreachable. While the chat server is
	// off the call returns without sending anything, like Java's isUp() check.
	chatserver::ChatServer::getInstance().sendPlayerLoginRequest(*getConnection()->getActivePlayer());
}

AION_CLIENT_PACKET(CM_CHAT_AUTH);

} // namespace aion::gameserver::network::aion::clientpackets
