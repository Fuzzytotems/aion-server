#include "aion/gameserver/network/aion/clientpackets/CM_MAY_LOGIN_INTO_GAME.h"

#include <memory>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAY_LOGIN_INTO_GAME.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_MAY_LOGIN_INTO_GAME::CM_MAY_LOGIN_INTO_GAME(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_MAY_LOGIN_INTO_GAME::readImpl() {
	// empty
}

void CM_MAY_LOGIN_INTO_GAME::runImpl() {
	const std::shared_ptr<AionConnection>& con = getConnection();
	// TODO! check if may login into game [play time etc]
	con->sendPacket(serverpackets::SM_MAY_LOGIN_INTO_GAME());
}

AION_CLIENT_PACKET(CM_MAY_LOGIN_INTO_GAME);

} // namespace aion::gameserver::network::aion::clientpackets
