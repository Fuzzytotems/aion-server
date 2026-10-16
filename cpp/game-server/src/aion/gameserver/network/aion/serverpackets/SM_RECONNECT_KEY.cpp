#include "aion/gameserver/network/aion/serverpackets/SM_RECONNECT_KEY.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECONNECT_KEY::SM_RECONNECT_KEY(int32_t keyValue)
	: AionServerPacket(opcodeOf<SM_RECONNECT_KEY>), key(keyValue) {
}

void SM_RECONNECT_KEY::writeImpl(AionConnection* con) {
	writeC(0x00);
	writeD(key);
}

} // namespace aion::gameserver::network::aion::serverpackets
