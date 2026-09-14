#include "aion/gameserver/network/aion/serverpackets/SM_NICKNAME_CHECK_RESPONSE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NICKNAME_CHECK_RESPONSE::SM_NICKNAME_CHECK_RESPONSE(int32_t valueValue)
	: AionServerPacket(opcodeOf<SM_NICKNAME_CHECK_RESPONSE>), value(valueValue) {
}

void SM_NICKNAME_CHECK_RESPONSE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
