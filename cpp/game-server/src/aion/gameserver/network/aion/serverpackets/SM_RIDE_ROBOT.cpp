#include "aion/gameserver/network/aion/serverpackets/SM_RIDE_ROBOT.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RIDE_ROBOT::SM_RIDE_ROBOT(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_RIDE_ROBOT>) {
	objectId = player.getObjectId();
	robotId = player.getRobotId();
}

SM_RIDE_ROBOT::SM_RIDE_ROBOT(model::gameobjects::player::Player& player, int32_t robotIdValue)
	: AionServerPacket(opcodeOf<SM_RIDE_ROBOT>), robotId(robotIdValue) {
	objectId = player.getObjectId();
}

void SM_RIDE_ROBOT::writeImpl(AionConnection* con) {
	writeD(objectId);
	writeD(robotId);
}

} // namespace aion::gameserver::network::aion::serverpackets
