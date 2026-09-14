#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUEST_ACTION::SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType actionTypeValue, questEngine::model::QuestState& qs)
	: AionServerPacket(opcodeOf<SM_QUEST_ACTION>), actionType(actionTypeValue) {
	AION_UNPORTED();
}

SM_QUEST_ACTION::SM_QUEST_ACTION(int32_t questIdValue)
	: AionServerPacket(opcodeOf<SM_QUEST_ACTION>), actionType(SM_QUEST_ACTION::ActionType::UNK), questId(questIdValue) {
}

SM_QUEST_ACTION::SM_QUEST_ACTION(int32_t questIdValue, int32_t timerValue)
	: AionServerPacket(opcodeOf<SM_QUEST_ACTION>), actionType(SM_QUEST_ACTION::ActionType::TIMER), questId(questIdValue), timer(timerValue) {
}

SM_QUEST_ACTION::SM_QUEST_ACTION(int32_t questIdValue, int32_t sharerIdValue, bool shareInAllianceValue)
	: AionServerPacket(opcodeOf<SM_QUEST_ACTION>), actionType(SM_QUEST_ACTION::ActionType::SHARE), questId(questIdValue), sharerId(sharerIdValue),
	  shareInAlliance(shareInAllianceValue) {
}

void SM_QUEST_ACTION::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
