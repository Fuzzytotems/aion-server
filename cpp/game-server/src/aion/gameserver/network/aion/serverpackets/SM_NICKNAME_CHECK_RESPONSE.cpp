#include "aion/gameserver/network/aion/serverpackets/SM_NICKNAME_CHECK_RESPONSE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NICKNAME_CHECK_RESPONSE::SM_NICKNAME_CHECK_RESPONSE(int32_t valueValue)
	: AionServerPacket(opcodeOf<SM_NICKNAME_CHECK_RESPONSE>), value(valueValue) {
}

void SM_NICKNAME_CHECK_RESPONSE::writeImpl(AionConnection* con) {
	// Here is some msg: 0x00 = ok 0x0A = not ok and much more
	writeC(value);
}

} // namespace aion::gameserver::network::aion::serverpackets
