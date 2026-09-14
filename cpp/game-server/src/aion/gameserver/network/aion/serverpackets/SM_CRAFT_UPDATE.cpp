#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CRAFT_UPDATE::SM_CRAFT_UPDATE(int32_t skillIdValue, const model::templates::item::ItemTemplate* item, int32_t successValue, int32_t failureValue,
	int32_t actionValue, int32_t executionSpeedValue, int32_t delayValue)
	: AionServerPacket(opcodeOf<SM_CRAFT_UPDATE>) {
	AION_UNPORTED();
}

void SM_CRAFT_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
