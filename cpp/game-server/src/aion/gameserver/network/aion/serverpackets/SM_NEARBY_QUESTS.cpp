#include "aion/gameserver/network/aion/serverpackets/SM_NEARBY_QUESTS.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NEARBY_QUESTS::SM_NEARBY_QUESTS(const std::unordered_map<int32_t, int32_t>& nearbyQuestListValue)
	: AionServerPacket(opcodeOf<SM_NEARBY_QUESTS>), nearbyQuestList(nearbyQuestListValue) {
}

void SM_NEARBY_QUESTS::writeImpl(AionConnection* con) {
	writeC(0);
	writeH(-static_cast<int32_t>(nearbyQuestList.size()) & 0xFFFF);
	// Java iterates the caller's HashMap (PlayerController.updateNearbyQuests)
	for (const auto& [key, value] : detail::javaHashMapOrder(nearbyQuestList)) {
		int32_t questId = key;
		if (value > 0)
			questId |= notYetAvailableBit; // for transparent/grey quest marker above npc's head
		writeD(questId);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
