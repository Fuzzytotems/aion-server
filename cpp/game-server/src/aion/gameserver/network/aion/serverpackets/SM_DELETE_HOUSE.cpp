#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_HOUSE::SM_DELETE_HOUSE(int32_t addressValue) : AionServerPacket(opcodeOf<SM_DELETE_HOUSE>), address(addressValue) {
}

void SM_DELETE_HOUSE::writeImpl(AionConnection* con) {
	writeD(address);
}

} // namespace aion::gameserver::network::aion::serverpackets
