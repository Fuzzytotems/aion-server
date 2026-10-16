#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_ACQUIRE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_ACQUIRE::SM_HOUSE_ACQUIRE(int32_t playerIdValue, int32_t addressValue, bool acquireValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_ACQUIRE>), playerId(playerIdValue), address(addressValue), acquire(acquireValue) {
}

void SM_HOUSE_ACQUIRE::writeImpl(AionConnection* con) {
	writeD(playerId);
	writeD(address);
	writeD(acquire ? 1 : 0); // now it has value 2 sometimes, maybe initial door state ?
}

} // namespace aion::gameserver::network::aion::serverpackets
