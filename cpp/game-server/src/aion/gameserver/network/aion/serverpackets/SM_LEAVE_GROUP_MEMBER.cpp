#include "aion/gameserver/network/aion/serverpackets/SM_LEAVE_GROUP_MEMBER.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEAVE_GROUP_MEMBER::SM_LEAVE_GROUP_MEMBER()
	: AionServerPacket(opcodeOf<SM_LEAVE_GROUP_MEMBER>) {
}

void SM_LEAVE_GROUP_MEMBER::writeImpl(AionConnection* con) {
	writeD(0x00);
	writeC(0x00);
	writeD(0x3F); // TODO: TeamType.getType
	writeD(0x00); // TODO: TeamType.getSubType
	writeH(0x00);
}

} // namespace aion::gameserver::network::aion::serverpackets
