#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_NOTE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UPDATE_NOTE::SM_UPDATE_NOTE(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_UPDATE_NOTE>) {
	AION_UNPORTED();
}

void SM_UPDATE_NOTE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
