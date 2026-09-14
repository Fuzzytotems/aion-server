#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_BLOCK_LIST::SM_BLOCK_LIST() : AionServerPacket(opcodeOf<SM_BLOCK_LIST>) {
}

void SM_BLOCK_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
