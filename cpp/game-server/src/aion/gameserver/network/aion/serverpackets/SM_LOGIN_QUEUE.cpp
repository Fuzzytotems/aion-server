#include "aion/gameserver/network/aion/serverpackets/SM_LOGIN_QUEUE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LOGIN_QUEUE::SM_LOGIN_QUEUE()
	: AionServerPacket(opcodeOf<SM_LOGIN_QUEUE>), waitingPosition(5), waitingTime(60), waitingCount(50) {
}

void SM_LOGIN_QUEUE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
