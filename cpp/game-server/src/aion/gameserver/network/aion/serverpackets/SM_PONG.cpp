#include "aion/gameserver/network/aion/serverpackets/SM_PONG.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PONG::SM_PONG()
	: AionServerPacket(opcodeOf<SM_PONG>) {
}

void SM_PONG::writeImpl(AionConnection* con) {
	writeC(0x00);
	writeC(0x00);
}

} // namespace aion::gameserver::network::aion::serverpackets
