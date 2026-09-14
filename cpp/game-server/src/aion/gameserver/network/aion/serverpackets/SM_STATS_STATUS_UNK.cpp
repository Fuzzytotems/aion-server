#include "aion/gameserver/network/aion/serverpackets/SM_STATS_STATUS_UNK.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATS_STATUS_UNK::SM_STATS_STATUS_UNK(int32_t lvlValue, int32_t pointsValue)
	: AionServerPacket(opcodeOf<SM_STATS_STATUS_UNK>), lvl(lvlValue), points(pointsValue) {
}

void SM_STATS_STATUS_UNK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
