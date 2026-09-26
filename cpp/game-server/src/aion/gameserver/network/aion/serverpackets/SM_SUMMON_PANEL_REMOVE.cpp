#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL_REMOVE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_PANEL_REMOVE::SM_SUMMON_PANEL_REMOVE(int32_t skillIdValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_PANEL_REMOVE>), skillId(skillIdValue) {
}

void SM_SUMMON_PANEL_REMOVE::writeImpl(AionConnection* con) {
	writeH(skillId); // skillId
	if (skillId != 0)
		writeC(1); // unk = 1
	else
		writeC(0); // unk
}

} // namespace aion::gameserver::network::aion::serverpackets
