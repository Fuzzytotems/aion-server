#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_PANEL_REMOVE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_PANEL_REMOVE::SM_SUMMON_PANEL_REMOVE(int32_t skillIdValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_PANEL_REMOVE>), skillId(skillIdValue) {
}

void SM_SUMMON_PANEL_REMOVE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
