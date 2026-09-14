#include "aion/gameserver/network/aion/serverpackets/SM_LEAVE_GROUP_MEMBER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEAVE_GROUP_MEMBER::SM_LEAVE_GROUP_MEMBER()
	: AionServerPacket(opcodeOf<SM_LEAVE_GROUP_MEMBER>) {
}

void SM_LEAVE_GROUP_MEMBER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
