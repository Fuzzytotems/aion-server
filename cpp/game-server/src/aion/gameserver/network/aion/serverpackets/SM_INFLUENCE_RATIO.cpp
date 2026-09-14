#include "aion/gameserver/network/aion/serverpackets/SM_INFLUENCE_RATIO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INFLUENCE_RATIO::SM_INFLUENCE_RATIO() : AionServerPacket(opcodeOf<SM_INFLUENCE_RATIO>) {
}

void SM_INFLUENCE_RATIO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
