#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHANNEL_INFO::SM_CHANNEL_INFO(runtime::Ptr<world::WorldPosition> position) : AionServerPacket(opcodeOf<SM_CHANNEL_INFO>) {
	AION_UNPORTED();
}

void SM_CHANNEL_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
