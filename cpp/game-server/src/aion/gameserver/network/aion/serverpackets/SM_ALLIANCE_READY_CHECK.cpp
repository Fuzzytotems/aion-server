#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_READY_CHECK.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ALLIANCE_READY_CHECK::SM_ALLIANCE_READY_CHECK(int32_t playerObjectIdValue, int32_t statusCodeValue)
	: AionServerPacket(opcodeOf<SM_ALLIANCE_READY_CHECK>), playerObjectId(playerObjectIdValue), statusCode(statusCodeValue) {
}

void SM_ALLIANCE_READY_CHECK::writeImpl(AionConnection* con) {
	writeD(playerObjectId);
	writeC(statusCode);
}

} // namespace aion::gameserver::network::aion::serverpackets
