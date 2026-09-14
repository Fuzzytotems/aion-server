#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_COMPLETED_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
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
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
