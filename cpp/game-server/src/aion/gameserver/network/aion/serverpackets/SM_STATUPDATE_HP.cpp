#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_HP.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATUPDATE_HP::SM_STATUPDATE_HP(int32_t currentHpValue, int32_t maxHpValue)
	: AionServerPacket(opcodeOf<SM_STATUPDATE_HP>), currentHp(currentHpValue), maxHp(maxHpValue) {
}

void SM_STATUPDATE_HP::writeImpl(AionConnection* con) {
	writeD(currentHp);
	writeD(maxHp);
}

} // namespace aion::gameserver::network::aion::serverpackets
