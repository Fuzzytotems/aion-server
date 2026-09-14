#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_MP.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATUPDATE_MP::SM_STATUPDATE_MP(int32_t currentMpValue, int32_t maxMpValue)
	: AionServerPacket(opcodeOf<SM_STATUPDATE_MP>), currentMp(currentMpValue), maxMp(maxMpValue) {
}

void SM_STATUPDATE_MP::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
