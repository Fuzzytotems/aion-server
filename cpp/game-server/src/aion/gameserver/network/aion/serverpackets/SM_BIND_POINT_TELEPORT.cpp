#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_BIND_POINT_TELEPORT::SM_BIND_POINT_TELEPORT(int32_t actionValue, int32_t playerIdValue, int32_t locIdValue, int32_t cooldownValue)
	: AionServerPacket(opcodeOf<SM_BIND_POINT_TELEPORT>), action(actionValue), playerId(playerIdValue), locId(locIdValue), cooldown(cooldownValue) {
}

void SM_BIND_POINT_TELEPORT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
