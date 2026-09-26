#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_BIND_POINT_TELEPORT::SM_BIND_POINT_TELEPORT(int32_t actionValue, int32_t playerIdValue, int32_t locIdValue, int32_t cooldownValue)
	: AionServerPacket(opcodeOf<SM_BIND_POINT_TELEPORT>), action(actionValue), playerId(playerIdValue), locId(locIdValue), cooldown(cooldownValue) {
}

void SM_BIND_POINT_TELEPORT::writeImpl(AionConnection* con) {
	writeC(action);
	writeD(playerId);
	switch (action) {
		case 1:
			writeD(locId);
			break;
		case 3:
			writeD(locId);
			writeD(cooldown);
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
