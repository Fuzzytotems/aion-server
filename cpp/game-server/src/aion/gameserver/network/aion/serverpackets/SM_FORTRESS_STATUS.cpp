#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_STATUS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FORTRESS_STATUS::SM_FORTRESS_STATUS() : AionServerPacket(opcodeOf<SM_FORTRESS_STATUS>) {
}

void SM_FORTRESS_STATUS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
