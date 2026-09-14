#include "aion/gameserver/network/aion/serverpackets/SM_UNWRAP_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UNWRAP_ITEM::SM_UNWRAP_ITEM(int32_t objectIdValue, int32_t countValue)
	: AionServerPacket(opcodeOf<SM_UNWRAP_ITEM>), objectId(objectIdValue), count(countValue) {
}

void SM_UNWRAP_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
