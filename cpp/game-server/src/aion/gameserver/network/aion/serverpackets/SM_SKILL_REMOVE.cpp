#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_REMOVE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SKILL_REMOVE::SM_SKILL_REMOVE(model::skill::PlayerSkillEntry& skill)
	: AionServerPacket(opcodeOf<SM_SKILL_REMOVE>) {
	AION_UNPORTED();
}

void SM_SKILL_REMOVE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
