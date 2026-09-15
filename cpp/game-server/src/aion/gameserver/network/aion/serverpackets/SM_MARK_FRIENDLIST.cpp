#include "aion/gameserver/network/aion/serverpackets/SM_MARK_FRIENDLIST.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MARK_FRIENDLIST::SM_MARK_FRIENDLIST()
	: AionServerPacket(opcodeOf<SM_MARK_FRIENDLIST>) {
}

void SM_MARK_FRIENDLIST::writeImpl(AionConnection* con) {
	writeD(detail::requireConnection(con, "SM_MARK_FRIENDLIST").getActivePlayer()->getObjectId());
	writeC(1);
	writeH(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
