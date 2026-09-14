#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_REPEAT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUEST_REPEAT::SM_QUEST_REPEAT(const std::vector<int32_t>& repeatableQuestsValue)
	: AionServerPacket(opcodeOf<SM_QUEST_REPEAT>), repeatableQuests(repeatableQuestsValue) {
}

void SM_QUEST_REPEAT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
