#include "aion/gameserver/network/aion/serverpackets/SM_DIE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DIE::SM_DIE(model::gameobjects::player::Player& player) : AionServerPacket(opcodeOf<SM_DIE>) {
	AION_UNPORTED();
}

void SM_DIE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

world::WorldMapType SM_DIE::getInvasionWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
