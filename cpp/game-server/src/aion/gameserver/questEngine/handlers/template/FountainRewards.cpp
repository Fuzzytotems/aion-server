#include "aion/gameserver/questEngine/handlers/template/FountainRewards.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::QuestService;

FountainRewards::FountainRewards(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue)
	: AbstractTemplateQuestHandler(questIdValue) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
}

void FountainRewards::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
}

bool FountainRewards::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	int32_t targetId = env.getTargetId();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.contains(targetId)) { // Coin Fountain
			switch (dialogActionId) {
				case DialogAction::USE_OBJECT:
					if (!QuestService::inventoryItemCheck(env, true)) {
						return true;
					} else
						return sendQuestSelectionDialog(env);
				case DialogAction::SETPRO1:
					if (QuestService::collectItemCheck(env, false)) {
						if (!player->getInventory().isFullSpecialCube()) {
							if (QuestService::startQuest(env)) {
								changeQuestStep(env, 0, 0, true);
								return sendQuestDialog(env, 5);
							}
						} else {
							utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY());
							return sendQuestSelectionDialog(env);
						}
					} else {
						return sendQuestSelectionDialog(env);
					}
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (startNpcIds.contains(targetId)) { // Coin Fountain
			if (dialogActionId == DialogAction::SELECTED_QUEST_NOREWARD) {
				if (QuestService::collectItemCheck(env, true))
					return sendQuestEndDialog(env);
			} else {
				return QuestService::abandonQuest(*player, questId);
			}
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
