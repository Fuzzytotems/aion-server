#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_ACTIVATION.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_ACTIVATION::SM_SKILL_ACTIVATION(int32_t skillIdValue, bool isActiveValue)
	: AionServerPacket(opcodeOf<SM_SKILL_ACTIVATION>), isActive(isActiveValue), unk(0), skillId(skillIdValue) {
}

SM_SKILL_ACTIVATION::SM_SKILL_ACTIVATION(int32_t skillIdValue)
	: AionServerPacket(opcodeOf<SM_SKILL_ACTIVATION>), isActive(true), unk(1), skillId(skillIdValue) {
}

void SM_SKILL_ACTIVATION::writeImpl(AionConnection* con) {
	writeH(skillId);
	writeD(unk);
	writeC(isActive ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
