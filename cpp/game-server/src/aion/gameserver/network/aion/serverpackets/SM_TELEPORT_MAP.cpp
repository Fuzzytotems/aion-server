#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_MAP.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TELEPORT_MAP::SM_TELEPORT_MAP(int32_t targetObjIdValue, int32_t teleportIdValue)
	: AionServerPacket(opcodeOf<SM_TELEPORT_MAP>), targetObjId(targetObjIdValue), teleportId(teleportIdValue) {
}

void SM_TELEPORT_MAP::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
