#include "aion/gameserver/network/aion/serverpackets/SM_PRICES.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PRICES::SM_PRICES()
	: AionServerPacket(opcodeOf<SM_PRICES>) {
}

void SM_PRICES::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
