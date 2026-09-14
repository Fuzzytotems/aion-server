#include "aion/gameserver/network/aion/serverpackets/SM_KEY.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_KEY::SM_KEY() : AionServerPacket(opcodeOf<SM_KEY>) {
}

void SM_KEY::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
