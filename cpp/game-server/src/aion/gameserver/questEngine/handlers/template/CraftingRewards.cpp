#include "aion/gameserver/questEngine/handlers/template/CraftingRewards.h"

#include <string>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;

CraftingRewards::CraftingRewards(int32_t questIdValue, int32_t startNpcIdValue, int32_t skillIdValue, int32_t levelRewardValue,
	int32_t endNpcIdValue, int32_t questMovieValue)
	: AbstractTemplateQuestHandler(questIdValue), startNpcId(startNpcIdValue), endNpcId(endNpcIdValue != 0 ? endNpcIdValue : startNpcIdValue),
	  skillId(skillIdValue), levelReward(levelRewardValue), questMovie(questMovieValue),
	  isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
}

void CraftingRewards::register_() {
	if (startNpcId != 0) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (endNpcId != startNpcId) {
		qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
	}
}

bool CraftingRewards::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (targetId == startNpcId && canLearn(*player)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, isDataDriven ? 4762 : 1011);
				default:
					return sendQuestStartDialog(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		if (targetId == endNpcId && canLearn(*player)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, isDataDriven ? 1011 : 2375);
				case DialogAction::SELECT_QUEST_REWARD:
					qs->setQuestVar(0);
					qs->setStatus(QuestStatus::REWARD);
					updateQuestStatus(env);
					player->getSkillList()->addSkill(*player, skillId, levelReward);
					if (questMovie != 0)
						playQuestMovie(env, questMovie);
					return sendQuestEndDialog(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (targetId == endNpcId) {
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool CraftingRewards::canLearn(Player& player) {
	if (levelReward == 400)
		return services::craft::CraftSkillUpdateService::getInstance().canLearnMoreExpertCraftingSkill(player);
	if (levelReward == 500)
		return services::craft::CraftSkillUpdateService::getInstance().canLearnMoreMasterCraftingSkill(player);
	throw runtime::IllegalStateException("Unhandled levelReward " + std::to_string(levelReward));
}

} // namespace aion::gameserver::questEngine::handlers::template_
