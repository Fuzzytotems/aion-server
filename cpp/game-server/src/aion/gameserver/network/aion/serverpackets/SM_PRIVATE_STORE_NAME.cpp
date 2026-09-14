#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE_NAME.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PRIVATE_STORE_NAME::SM_PRIVATE_STORE_NAME(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_PRIVATE_STORE_NAME>) {
	AION_UNPORTED();
}

void SM_PRIVATE_STORE_NAME::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
