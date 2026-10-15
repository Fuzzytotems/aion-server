#include "aion/gameserver/questEngine/handlers/template/RelicRewards.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using services::QuestService;

RelicRewards::RelicRewards(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue)
	: AbstractTemplateQuestHandler(questIdValue), isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
}

void RelicRewards::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
}

bool RelicRewards::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.contains(targetId)) {
			switch (dialogActionId) {
				case DialogAction::EXCHANGE_COIN: {
					const gameserver::model::templates::QuestTemplate& template_ = questTemplateOf(env.getQuestId());
					if (player->getCommonData()->getLevel() >= template_.getMinlevelPermitted()) {
						if (QuestService::checkAndGetCollectItemQuestRewardCategory(env) != -1)
							return sendQuestDialog(env, isDataDriven ? 4762 : 1011);
						else
							return sendQuestDialog(env, 3398);
					} else
						return sendQuestDialog(env, 3398);
				}
			}
		}
	} else if (qs->getStatus() == QuestStatus::START && qs->getQuestVarById(0) == 0) {
		if (startNpcIds.contains(targetId)) {
			int32_t rewardId = -1;
			switch (dialogActionId) {
				case DialogAction::USE_OBJECT:
					return sendQuestDialog(env, isDataDriven ? 4762 : 1011);
				case DialogAction::SELECT1:
					rewardId = QuestService::checkAndGetCollectItemQuestRewardCategory(env, 0);
					break;
				case DialogAction::SELECT2:
					rewardId = QuestService::checkAndGetCollectItemQuestRewardCategory(env, 1);
					break;
				case DialogAction::SELECT3:
					rewardId = QuestService::checkAndGetCollectItemQuestRewardCategory(env, 2);
					break;
				case DialogAction::SELECT4:
					rewardId = QuestService::checkAndGetCollectItemQuestRewardCategory(env, 3);
					break;
			}
			if (rewardId != -1) {
				qs->setRewardGroup(rewardId);
				qs->setQuestVar(rewardId + 1);
				qs->setStatus(QuestStatus::REWARD);
				updateQuestStatus(env);
				return sendQuestDialog(env, rewardId + 5);
			} else
				return sendQuestDialog(env, 1009);
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (startNpcIds.contains(targetId)) {
			int32_t var = qs->getQuestVarById(0);
			switch (dialogActionId) {
				case DialogAction::USE_OBJECT:
					return sendQuestDialog(env, var + 4);
				case DialogAction::SELECTED_QUEST_NOREWARD:
					sendQuestEndDialog(env);
					return true;
			}
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
