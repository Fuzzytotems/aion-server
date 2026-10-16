#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestStatusOperation.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

using network::aion::serverpackets::SM_QUEST_ACTION;

void SetQuestStatusOperation::doOperate(model::QuestEnv& env) const {
	runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
	int32_t questId = env.getQuestId();
	runtime::Ptr<model::QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs) {
		qs->setStatus(status);
		utils::PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
		if (qs->getStatus() == model::QuestStatus::COMPLETE)
			player->getController().updateNearbyQuests();
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
