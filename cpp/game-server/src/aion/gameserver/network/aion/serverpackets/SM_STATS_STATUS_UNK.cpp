#include "aion/gameserver/network/aion/serverpackets/SM_STATS_STATUS_UNK.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATS_STATUS_UNK::SM_STATS_STATUS_UNK(int32_t lvlValue, int32_t pointsValue)
	: AionServerPacket(opcodeOf<SM_STATS_STATUS_UNK>), lvl(lvlValue), points(pointsValue) {
}

void SM_STATS_STATUS_UNK::writeImpl(AionConnection* con) {
	writeD(points);
	writeC(1);
	if (lvl == 50)
		writeC(1);
	else
		writeC(2);
	writeD(lvl);
	writeD(lvl);
	writeD(lvl == 50 ? 1 : 0);
	writeC(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
