#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_LOOT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GROUP_LOOT::SM_GROUP_LOOT(int32_t groupIdValue, int32_t playerIdValue, int32_t itemIdValue, int32_t itemCountValue, int32_t lootCorpseIdValue,
	int32_t distributionIdValue, int64_t luckValue, int32_t indexValue)
	: AionServerPacket(opcodeOf<SM_GROUP_LOOT>), groupId(groupIdValue), index(indexValue), itemCount(itemCountValue), itemId(itemIdValue), unk3(0),
	  lootCorpseId(lootCorpseIdValue), distributionId(distributionIdValue), playerId(playerIdValue), luck(luckValue) {
}

void SM_GROUP_LOOT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
