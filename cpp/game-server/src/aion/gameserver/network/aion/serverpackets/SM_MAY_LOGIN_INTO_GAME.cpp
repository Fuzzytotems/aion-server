#include "aion/gameserver/network/aion/serverpackets/SM_MAY_LOGIN_INTO_GAME.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MAY_LOGIN_INTO_GAME::SM_MAY_LOGIN_INTO_GAME()
	: AionServerPacket(opcodeOf<SM_MAY_LOGIN_INTO_GAME>) {
}

void SM_MAY_LOGIN_INTO_GAME::writeImpl(AionConnection* con) {
	// probably here is msg if fail.
	writeD(0x00);
}

} // namespace aion::gameserver::network::aion::serverpackets
