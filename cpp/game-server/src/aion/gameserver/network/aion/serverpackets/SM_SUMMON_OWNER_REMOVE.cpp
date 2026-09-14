#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_OWNER_REMOVE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SUMMON_OWNER_REMOVE::SM_SUMMON_OWNER_REMOVE(int32_t summonObjIdValue)
	: AionServerPacket(opcodeOf<SM_SUMMON_OWNER_REMOVE>), summonObjId(summonObjIdValue) {
}

void SM_SUMMON_OWNER_REMOVE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
