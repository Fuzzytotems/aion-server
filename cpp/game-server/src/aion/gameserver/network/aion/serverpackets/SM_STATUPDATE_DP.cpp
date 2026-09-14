#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_DP.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATUPDATE_DP::SM_STATUPDATE_DP(int32_t currentDpValue)
	: AionServerPacket(opcodeOf<SM_STATUPDATE_DP>), currentDp(currentDpValue) {
}

void SM_STATUPDATE_DP::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
