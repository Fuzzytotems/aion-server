#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM_ANNOUNCE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WINDSTREAM_ANNOUNCE::SM_WINDSTREAM_ANNOUNCE(int32_t bidirectionalValue, int32_t mapIdValue, int32_t streamIdValue, int32_t stateValue)
	: AionServerPacket(opcodeOf<SM_WINDSTREAM_ANNOUNCE>), bidirectional(bidirectionalValue), mapId(mapIdValue), streamId(streamIdValue),
	  state(stateValue) {
}

void SM_WINDSTREAM_ANNOUNCE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
