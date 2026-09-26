#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_INFO.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FORTRESS_INFO::SM_FORTRESS_INFO(int32_t locationIdValue, bool teleportStatusValue)
	: AionServerPacket(opcodeOf<SM_FORTRESS_INFO>), locationId(locationIdValue), teleportStatus(teleportStatusValue) {
}

void SM_FORTRESS_INFO::writeImpl(AionConnection* con) {
	writeD(locationId);
	writeC(teleportStatus ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
