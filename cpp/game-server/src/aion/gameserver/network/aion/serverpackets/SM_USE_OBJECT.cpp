#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_USE_OBJECT::SM_USE_OBJECT(int32_t playerObjIdValue, int32_t targetObjIdValue, int32_t timeValue, int32_t actionTypeValue)
	: AionServerPacket(opcodeOf<SM_USE_OBJECT>), playerObjId(playerObjIdValue), targetObjId(targetObjIdValue), time(timeValue),
	  actionType(actionTypeValue) {
}

void SM_USE_OBJECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
