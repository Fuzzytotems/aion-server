#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TELEPORT_LOC::SM_TELEPORT_LOC(int32_t mapIdValue, int32_t instanceIdValue, float xValue, float yValue, float zValue, int8_t headingValue,
	model::animations::TeleportAnimation portAnimationValue)
	: AionServerPacket(opcodeOf<SM_TELEPORT_LOC>), mapId(mapIdValue), instanceId(instanceIdValue), x(xValue), y(yValue), z(zValue),
	  heading(headingValue) {
	AION_UNPORTED();
}

void SM_TELEPORT_LOC::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
