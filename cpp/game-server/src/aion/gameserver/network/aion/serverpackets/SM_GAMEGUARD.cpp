#include "aion/gameserver/network/aion/serverpackets/SM_GAMEGUARD.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GAMEGUARD::SM_GAMEGUARD(int32_t sizeValue) : AionServerPacket(opcodeOf<SM_GAMEGUARD>), size(sizeValue) {
}

void SM_GAMEGUARD::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
