#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_TIME_CHECK_4_7_5.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_AFTER_TIME_CHECK_4_7_5::SM_AFTER_TIME_CHECK_4_7_5() : AionServerPacket(opcodeOf<SM_AFTER_TIME_CHECK_4_7_5>) {
}

void SM_AFTER_TIME_CHECK_4_7_5::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
