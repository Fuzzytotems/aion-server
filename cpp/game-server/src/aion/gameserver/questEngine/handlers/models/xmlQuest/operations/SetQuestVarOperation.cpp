#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestVarOperation.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

using network::aion::serverpackets::SM_QUEST_ACTION;

void SetQuestVarOperation::doOperate(model::QuestEnv& env) const {
	runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
	int32_t questId = env.getQuestId();
	runtime::Ptr<model::QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs) {
		qs->getQuestVars()->setVarById(varId, value);
		utils::PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
