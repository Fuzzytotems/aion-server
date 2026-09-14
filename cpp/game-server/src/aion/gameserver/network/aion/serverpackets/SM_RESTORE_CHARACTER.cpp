#include "aion/gameserver/network/aion/serverpackets/SM_RESTORE_CHARACTER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RESTORE_CHARACTER::SM_RESTORE_CHARACTER(int32_t chaOidValue, bool successValue)
	: AionServerPacket(opcodeOf<SM_RESTORE_CHARACTER>), chaOid(chaOidValue), success(successValue) {
}

void SM_RESTORE_CHARACTER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
