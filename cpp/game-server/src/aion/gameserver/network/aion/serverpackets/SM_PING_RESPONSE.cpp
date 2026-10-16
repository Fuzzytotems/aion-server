#include "aion/gameserver/network/aion/serverpackets/SM_PING_RESPONSE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PING_RESPONSE::SM_PING_RESPONSE()
	: AionServerPacket(opcodeOf<SM_PING_RESPONSE>) {
}

void SM_PING_RESPONSE::writeImpl(AionConnection* con) {
	writeC(0x04);
}

} // namespace aion::gameserver::network::aion::serverpackets
