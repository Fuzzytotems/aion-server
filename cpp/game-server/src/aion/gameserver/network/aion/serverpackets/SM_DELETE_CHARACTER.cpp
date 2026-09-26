#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_CHARACTER.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_CHARACTER::SM_DELETE_CHARACTER(int32_t playerObjIdValue, int32_t deletionTimeValue)
	: AionServerPacket(opcodeOf<SM_DELETE_CHARACTER>), playerObjId(playerObjIdValue), deletionTime(deletionTimeValue) {
}

void SM_DELETE_CHARACTER::writeImpl(AionConnection* con) {
	if (playerObjId != 0) {
		writeD(0x00); // unk
		writeD(playerObjId);
		writeD(deletionTime);
	} else {
		writeD(0x10); // unk
		writeD(0x00);
		writeD(0x00);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
