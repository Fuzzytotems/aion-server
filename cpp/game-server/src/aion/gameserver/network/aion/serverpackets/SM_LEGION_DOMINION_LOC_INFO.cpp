#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_LOC_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_DOMINION_LOC_INFO::SM_LEGION_DOMINION_LOC_INFO()
	: AionServerPacket(opcodeOf<SM_LEGION_DOMINION_LOC_INFO>) {
}

void SM_LEGION_DOMINION_LOC_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
