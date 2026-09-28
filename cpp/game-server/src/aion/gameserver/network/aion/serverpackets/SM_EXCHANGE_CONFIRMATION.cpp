#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_CONFIRMATION.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EXCHANGE_CONFIRMATION::SM_EXCHANGE_CONFIRMATION(int32_t actionValue) : AionServerPacket(opcodeOf<SM_EXCHANGE_CONFIRMATION>), action(actionValue) {
}

void SM_EXCHANGE_CONFIRMATION::writeImpl(AionConnection* con) {
	writeC(action);
}

} // namespace aion::gameserver::network::aion::serverpackets
