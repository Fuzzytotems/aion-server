#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_LOOT.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GROUP_LOOT::SM_GROUP_LOOT(int32_t groupIdValue, int32_t playerIdValue, int32_t itemIdValue, int32_t itemCountValue, int32_t lootCorpseIdValue,
	int32_t distributionIdValue, int64_t luckValue, int32_t indexValue)
	: AionServerPacket(opcodeOf<SM_GROUP_LOOT>), groupId(groupIdValue), index(indexValue), itemCount(itemCountValue), itemId(itemIdValue), unk3(0),
	  lootCorpseId(lootCorpseIdValue), distributionId(distributionIdValue), playerId(playerIdValue), luck(luckValue) {
}

void SM_GROUP_LOOT::writeImpl(AionConnection* con) {
	writeD(groupId);
	writeD(index);
	writeD(itemCount);
	writeD(itemId);
	writeC(unk3);
	writeC(0); // 3.0
	writeC(0); // 3.5
	writeD(lootCorpseId);
	writeC(distributionId);
	writeD(playerId); // 0 starts the roll option
	writeD(static_cast<int32_t>(luck));
}

} // namespace aion::gameserver::network::aion::serverpackets
