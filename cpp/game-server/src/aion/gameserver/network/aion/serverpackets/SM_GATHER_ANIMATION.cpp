#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_ANIMATION.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GATHER_ANIMATION::SM_GATHER_ANIMATION(int32_t playerObjIdValue, int32_t gatherableObjIdValue, int32_t skillIdValue, int32_t actionValue)
	: AionServerPacket(opcodeOf<SM_GATHER_ANIMATION>), playerObjId(playerObjIdValue), gatherableObjId(gatherableObjIdValue), skillId(skillIdValue),
	  action(actionValue) {
}

void SM_GATHER_ANIMATION::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
