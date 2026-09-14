#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WINDSTREAM::SM_WINDSTREAM(int32_t unk1Value, int32_t unk2Value)
	: AionServerPacket(opcodeOf<SM_WINDSTREAM>), unk1(unk1Value), unk2(unk2Value) {
}

void SM_WINDSTREAM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
