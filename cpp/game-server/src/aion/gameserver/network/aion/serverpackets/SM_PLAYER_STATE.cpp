#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_STATE::SM_PLAYER_STATE(model::gameobjects::Creature& creature)
	: AionServerPacket(opcodeOf<SM_PLAYER_STATE>) {
	AION_UNPORTED();
}

void SM_PLAYER_STATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
