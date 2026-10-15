#include "aion/gameserver/network/aion/serverpackets/SM_POSITION_SELF.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_POSITION_SELF::SM_POSITION_SELF(float xValue, float yValue, float zValue, int8_t headingValue)
	: AionServerPacket(opcodeOf<SM_POSITION_SELF>), x(xValue), y(yValue), z(zValue), heading(headingValue) {
}

void SM_POSITION_SELF::writeImpl(AionConnection* con) {
	writeF(x);
	writeF(y);
	writeF(z);
	writeC(heading);
}

} // namespace aion::gameserver::network::aion::serverpackets
