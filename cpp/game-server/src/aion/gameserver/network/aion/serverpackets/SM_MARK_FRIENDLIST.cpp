#include "aion/gameserver/network/aion/serverpackets/SM_MARK_FRIENDLIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MARK_FRIENDLIST::SM_MARK_FRIENDLIST()
	: AionServerPacket(opcodeOf<SM_MARK_FRIENDLIST>) {
}

void SM_MARK_FRIENDLIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
