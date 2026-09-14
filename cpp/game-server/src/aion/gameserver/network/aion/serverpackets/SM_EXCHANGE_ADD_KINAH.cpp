#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_KINAH.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EXCHANGE_ADD_KINAH::SM_EXCHANGE_ADD_KINAH(int64_t kinahCountValue, int32_t actionValue)
	: AionServerPacket(opcodeOf<SM_EXCHANGE_ADD_KINAH>), kinahCount(kinahCountValue), action(actionValue) {
}

void SM_EXCHANGE_ADD_KINAH::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
