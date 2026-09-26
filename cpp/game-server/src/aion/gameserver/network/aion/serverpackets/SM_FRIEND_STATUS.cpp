#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_STATUS.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FRIEND_STATUS::SM_FRIEND_STATUS(int32_t statusValue) : AionServerPacket(opcodeOf<SM_FRIEND_STATUS>), status(statusValue) {
}

void SM_FRIEND_STATUS::writeImpl(AionConnection* con) {
	writeC(status);
}

} // namespace aion::gameserver::network::aion::serverpackets
