#include "aion/gameserver/network/aion/serverpackets/SM_PING_RESPONSE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PING_RESPONSE::SM_PING_RESPONSE()
	: AionServerPacket(opcodeOf<SM_PING_RESPONSE>) {
}

void SM_PING_RESPONSE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
