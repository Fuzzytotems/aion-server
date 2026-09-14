#include "aion/gameserver/network/aion/serverpackets/SM_POSITION_SELF.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_POSITION_SELF::SM_POSITION_SELF(float xValue, float yValue, float zValue, int8_t headingValue)
	: AionServerPacket(opcodeOf<SM_POSITION_SELF>), x(xValue), y(yValue), z(zValue), heading(headingValue) {
}

void SM_POSITION_SELF::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
