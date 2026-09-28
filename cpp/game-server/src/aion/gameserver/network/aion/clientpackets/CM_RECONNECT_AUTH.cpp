#include "aion/gameserver/network/aion/clientpackets/CM_RECONNECT_AUTH.h"

#include <memory>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_RECONNECT_AUTH::CM_RECONNECT_AUTH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_RECONNECT_AUTH::readImpl() {
}

void CM_RECONNECT_AUTH::runImpl() {
	const std::shared_ptr<AionConnection>& con = getConnection();
	loginserver::LoginServer::getInstance().requestAuthReconnection(con->getAccount()->getId(), con.get());
}

AION_CLIENT_PACKET(CM_RECONNECT_AUTH);

} // namespace aion::gameserver::network::aion::clientpackets
