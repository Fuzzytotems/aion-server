#include "aion/gameserver/network/aion/serverpackets/SM_FLY_TIME.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FLY_TIME::SM_FLY_TIME(int32_t currentFpValue, int32_t maxFpValue)
	: AionServerPacket(opcodeOf<SM_FLY_TIME>), currentFp(currentFpValue), maxFp(maxFpValue) {
}

void SM_FLY_TIME::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
