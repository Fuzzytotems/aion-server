#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_ANIMATION.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CRAFT_ANIMATION::SM_CRAFT_ANIMATION(int32_t playerObjIdValue, int32_t targetObjectIdValue, int32_t skillIdValue, int32_t actionValue)
	: AionServerPacket(opcodeOf<SM_CRAFT_ANIMATION>), playerObjId(playerObjIdValue), targetObjectId(targetObjectIdValue), skillId(skillIdValue),
	  action(actionValue) {
}

void SM_CRAFT_ANIMATION::writeImpl(AionConnection* con) {
	writeD(playerObjId);
	writeD(targetObjectId);
	writeH(skillId);
	writeC(action);
}

} // namespace aion::gameserver::network::aion::serverpackets
