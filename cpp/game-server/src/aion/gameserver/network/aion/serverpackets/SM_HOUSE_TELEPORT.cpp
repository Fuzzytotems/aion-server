#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_TELEPORT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_TELEPORT::SM_HOUSE_TELEPORT(int32_t houseAddress, int32_t playerIdValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_TELEPORT>), address(houseAddress), playerId(playerIdValue) {
}

void SM_HOUSE_TELEPORT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
