#include "aion/gameserver/network/aion/serverpackets/SM_RIDE_ROBOT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RIDE_ROBOT::SM_RIDE_ROBOT(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_RIDE_ROBOT>) {
	AION_UNPORTED();
}

SM_RIDE_ROBOT::SM_RIDE_ROBOT(model::gameobjects::player::Player& player, int32_t robotIdValue)
	: AionServerPacket(opcodeOf<SM_RIDE_ROBOT>), robotId(robotIdValue) {
	AION_UNPORTED();
}

void SM_RIDE_ROBOT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
