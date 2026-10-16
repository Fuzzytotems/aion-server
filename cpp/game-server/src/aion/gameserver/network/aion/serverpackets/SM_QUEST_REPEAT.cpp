#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_REPEAT.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUEST_REPEAT::SM_QUEST_REPEAT(const std::vector<int32_t>& repeatableQuestsValue)
	: AionServerPacket(opcodeOf<SM_QUEST_REPEAT>), repeatableQuests(repeatableQuestsValue) {
}

void SM_QUEST_REPEAT::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(repeatableQuests.size()));
	for (int32_t questId : repeatableQuests)
		writeD(questId);
	// There are following messages after this packet:
	// You can receive the daily quest. - STR_MSG_QUEST_LIMIT_RESET_DAILY = 1400854
	// You can receive the daily quest again at %0 in the morning. - STR_MSG_QUEST_LIMIT_START_DAILY = 1400855
	// You can receive the weekly quest. - STR_MSG_QUEST_LIMIT_RESET_WEEK = 1400856
	// You can receive the weekly quest again at %1 in the morning on %0. - STR_MSG_QUEST_LIMIT_START_WEEK = 1400857
}

} // namespace aion::gameserver::network::aion::serverpackets
