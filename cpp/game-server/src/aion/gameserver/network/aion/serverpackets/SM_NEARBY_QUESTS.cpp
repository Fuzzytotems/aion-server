#include "aion/gameserver/network/aion/serverpackets/SM_NEARBY_QUESTS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NEARBY_QUESTS::SM_NEARBY_QUESTS(const std::unordered_map<int32_t, int32_t>& nearbyQuestListValue)
	: AionServerPacket(opcodeOf<SM_NEARBY_QUESTS>), nearbyQuestList(nearbyQuestListValue) {
}

void SM_NEARBY_QUESTS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
