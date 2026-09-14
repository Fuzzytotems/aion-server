#include "aion/gameserver/network/aion/serverpackets/SM_TIME_CHECK.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TIME_CHECK::SM_TIME_CHECK(int32_t nanoTimeValue)
	: AionServerPacket(opcodeOf<SM_TIME_CHECK>), nanoTime(nanoTimeValue) {
	AION_UNPORTED();
}

void SM_TIME_CHECK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
