#include "aion/gameserver/network/aion/serverpackets/SM_GAME_TIME.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GAME_TIME::SM_GAME_TIME() : AionServerPacket(opcodeOf<SM_GAME_TIME>) {
}

void SM_GAME_TIME::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
