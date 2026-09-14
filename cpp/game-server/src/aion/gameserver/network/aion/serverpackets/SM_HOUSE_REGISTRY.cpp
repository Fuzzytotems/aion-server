#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_REGISTRY.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_REGISTRY::SM_HOUSE_REGISTRY(int32_t actionValue) : AionServerPacket(opcodeOf<SM_HOUSE_REGISTRY>), action(actionValue) {
}

void SM_HOUSE_REGISTRY::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
