#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FRIEND_LIST::SM_FRIEND_LIST() : AionServerPacket(opcodeOf<SM_FRIEND_LIST>) {
}

void SM_FRIEND_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
