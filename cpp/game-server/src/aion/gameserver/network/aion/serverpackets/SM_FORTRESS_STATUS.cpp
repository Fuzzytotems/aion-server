#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_STATUS.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FORTRESS_STATUS::SM_FORTRESS_STATUS() : AionServerPacket(opcodeOf<SM_FORTRESS_STATUS>) {
}

void SM_FORTRESS_STATUS::writeImpl(AionConnection* con) {
	// Java reads SiegeService.getFortresses(), getSecondsUntilNextFortressState() and Influence.getInstance(): model/siege/Influence.h (P5-12a)
	// is not written yet
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
