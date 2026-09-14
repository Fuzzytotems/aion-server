#include "aion/gameserver/network/aion/serverpackets/SM_UNK_3_5_1.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UNK_3_5_1::SM_UNK_3_5_1()
	: AionServerPacket(opcodeOf<SM_UNK_3_5_1>) {
}

void SM_UNK_3_5_1::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
