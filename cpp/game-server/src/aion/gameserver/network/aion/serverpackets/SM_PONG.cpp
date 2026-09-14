#include "aion/gameserver/network/aion/serverpackets/SM_PONG.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PONG::SM_PONG()
	: AionServerPacket(opcodeOf<SM_PONG>) {
}

void SM_PONG::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
