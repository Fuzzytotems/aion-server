#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LOOT_STATUS::SM_LOOT_STATUS(int32_t targetObjectIdValue, SM_LOOT_STATUS::Status statusValue)
	: AionServerPacket(opcodeOf<SM_LOOT_STATUS>), targetObjectId(targetObjectIdValue), status(statusValue) {
	AION_UNPORTED();
}

void SM_LOOT_STATUS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

int32_t SM_LOOT_STATUS::getLootEffect(int32_t value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
