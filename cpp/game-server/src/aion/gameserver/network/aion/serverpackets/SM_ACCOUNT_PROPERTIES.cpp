#include "aion/gameserver/network/aion/serverpackets/SM_ACCOUNT_PROPERTIES.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ACCOUNT_PROPERTIES::SM_ACCOUNT_PROPERTIES() : AionServerPacket(opcodeOf<SM_ACCOUNT_PROPERTIES>) {
}

void SM_ACCOUNT_PROPERTIES::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
