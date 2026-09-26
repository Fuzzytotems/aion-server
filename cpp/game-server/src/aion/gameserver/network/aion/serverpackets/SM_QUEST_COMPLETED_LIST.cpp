#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_COMPLETED_LIST.h"

#include <algorithm>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::network::aion::serverpackets {

// callback key com.aionemu.gameserver.network.aion.serverpackets.SM_QUEST_COMPLETED_LIST@L16:88 (captureless, ported)
const runtime::PinnedCallback<int32_t(questEngine::model::QuestState&)> SM_QUEST_COMPLETED_LIST::DYNAMIC_BODY_PART_SIZE_CALCULATOR(
	[](questEngine::model::QuestState&) -> int32_t { return 6; });

SM_QUEST_COMPLETED_LIST::SM_QUEST_COMPLETED_LIST(int32_t updateModeValue,
	const std::vector<runtime::Ptr<questEngine::model::QuestState>>& questStatesValue)
	: AionServerPacket(opcodeOf<SM_QUEST_COMPLETED_LIST>), updateMode(updateModeValue),
	  questStates(questStatesValue.begin(), questStatesValue.end()) {
}

SM_QUEST_COMPLETED_LIST::~SM_QUEST_COMPLETED_LIST() = default;

void SM_QUEST_COMPLETED_LIST::writeImpl(AionConnection* con) {
	writeC(1); // unk, always 1 (when 0, no entries change)
	writeC(updateMode); // 0 = rewrite all entries, 1 = insert new entries
	writeH(-static_cast<int32_t>(questStates.size()) & 0xFFFF);
	for (const runtime::Ref<questEngine::model::QuestState>& qs : questStates) {
		writeD(qs->getQuestId());
		writeC(std::min(qs->getCompleteCount(), 255));
		writeC(qs->canRepeat() ? 0 : 1); // wrong! most times equal to the complete count on retail (else 0), not clear what it is
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
