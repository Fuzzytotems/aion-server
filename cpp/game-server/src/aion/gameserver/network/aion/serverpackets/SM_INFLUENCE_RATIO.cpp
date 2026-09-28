#include "aion/gameserver/network/aion/serverpackets/SM_INFLUENCE_RATIO.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INFLUENCE_RATIO::SM_INFLUENCE_RATIO() : AionServerPacket(opcodeOf<SM_INFLUENCE_RATIO>) {
}

void SM_INFLUENCE_RATIO::writeImpl(AionConnection* con) {
	// Java reads Influence.getInstance() (rates and the influence per world) and SiegeService.getSecondsUntilNextFortressState():
	// model/siege/Influence.h (P5-12a) is not written yet
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
