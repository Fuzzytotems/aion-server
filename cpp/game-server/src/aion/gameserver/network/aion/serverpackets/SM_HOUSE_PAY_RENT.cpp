#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_PAY_RENT.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_PAY_RENT::SM_HOUSE_PAY_RENT(int32_t weeksPaidValue) : AionServerPacket(opcodeOf<SM_HOUSE_PAY_RENT>), weeksPaid(weeksPaidValue) {
}

void SM_HOUSE_PAY_RENT::writeImpl(AionConnection* con) {
	writeC(0);
	writeC(weeksPaid);
}

} // namespace aion::gameserver::network::aion::serverpackets
