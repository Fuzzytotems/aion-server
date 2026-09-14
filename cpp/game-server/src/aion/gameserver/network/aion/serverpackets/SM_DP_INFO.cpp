#include "aion/gameserver/network/aion/serverpackets/SM_DP_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DP_INFO::SM_DP_INFO(int32_t playerObjectIdValue, int32_t currentDpValue)
	: AionServerPacket(opcodeOf<SM_DP_INFO>), playerObjectId(playerObjectIdValue), currentDp(currentDpValue) {
}

void SM_DP_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
