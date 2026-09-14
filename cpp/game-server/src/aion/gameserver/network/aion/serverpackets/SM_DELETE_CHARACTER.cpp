#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_CHARACTER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_CHARACTER::SM_DELETE_CHARACTER(int32_t playerObjIdValue, int32_t deletionTimeValue)
	: AionServerPacket(opcodeOf<SM_DELETE_CHARACTER>), playerObjId(playerObjIdValue), deletionTime(deletionTimeValue) {
}

void SM_DELETE_CHARACTER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
