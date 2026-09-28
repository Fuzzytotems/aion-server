#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_LIST.h"

#include <algorithm>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUEST_LIST::SM_QUEST_LIST(const std::vector<runtime::Ptr<questEngine::model::QuestState>>& questState)
	: AionServerPacket(opcodeOf<SM_QUEST_LIST>), questStates(questState.begin(), questState.end()) {
}

SM_QUEST_LIST::~SM_QUEST_LIST() = default;

void SM_QUEST_LIST::writeImpl(AionConnection* con) {
	writeH(0x01); // unk
	writeH(-static_cast<int32_t>(questStates.size()) & 0xFFFF);
	for (const runtime::Ref<questEngine::model::QuestState>& qs : questStates) {
		writeD(qs->getQuestId());
		writeC(detail::questStatusValue(qs->getStatus()));
		writeD(qs->getQuestVars()->getQuestVars() | (qs->getFlags() << 24));
		writeC(std::min(qs->getCompleteCount(), 255));
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
