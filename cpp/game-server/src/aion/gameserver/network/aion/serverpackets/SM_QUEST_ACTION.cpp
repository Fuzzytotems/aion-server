#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: SM_QUEST_ACTION.ActionType.getId() - ADD(1), UPDATE(2), ABANDON(3), TIMER(4), SHARE(5), UNK(6): the ordinal + 1 */
int32_t actionTypeId(SM_QUEST_ACTION::ActionType actionType) {
	return static_cast<int32_t>(actionType) + 1;
}

} // namespace

SM_QUEST_ACTION::SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType actionTypeValue, questEngine::model::QuestState& qs)
	: AionServerPacket(opcodeOf<SM_QUEST_ACTION>), actionType(actionTypeValue) {
	questId = qs.getQuestId();
	status = detail::questStatusValue(qs.getStatus());
	step = qs.getQuestVars()->getQuestVars();
	flags = qs.getFlags();
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
	const model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (questTemplate != nullptr && questTemplate->getExtraCategory() != model::templates::quest::QuestExtraCategory::NONE)
		return;
	writeC(actionTypeId(actionType));
	writeD(questId);
	switch (actionType) {
		case ActionType::ADD:
			writeC(status); // quest status goes by ENUM value
			writeC(0x0);
			writeD(step | flags << 24); // current quest step
			writeH(0);
			writeC(0); // seen sometimes 1 for campaign quests
			break;
		case ActionType::UPDATE:
			writeC(status); // quest status goes by ENUM value
			writeC(0x0);
			writeD(step | flags << 24); // current quest step
			writeH(0); // seen sometimes 1 when status == COMPLETED
			break;
		case ActionType::ABANDON:
			writeD(0);
			break;
		case ActionType::TIMER:
			writeD(timer); // sets client timer ie 84030000 is 900 seconds/15 mins
			writeC(timer > 0 ? 1 : 0);
			break;
		case ActionType::SHARE:
			writeD(sharerId);
			writeD(shareInAlliance ? 1 : 0); // 0: group, 1: alliance
			break;
		case ActionType::UNK:
			writeH(0x01); // ???
			writeH(0x0);
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
