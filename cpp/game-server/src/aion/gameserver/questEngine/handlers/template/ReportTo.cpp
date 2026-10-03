#include "aion/gameserver/questEngine/handlers/template/ReportTo.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.ReportTo");

ReportTo::ReportTo(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, int32_t startDialogIdValue)
	: AbstractTemplateQuestHandler(questIdValue), startDialogId(startDialogIdValue), isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue)
		endNpcIds.addAll(*endNpcIdsValue);
	else
		endNpcIds.addAll(startNpcIds.snapshot());
	if (runtime::Ptr<runtime::RcArrayList<const gameserver::model::templates::quest::QuestItems*>> items = workItems.get()) {
		if (items->size() > 1)
			log.warn("Q{} has more than 1 work item", questId);
		workItem = items->get(0);
	}
}

void ReportTo::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds) {
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
		}
	}
}

bool ReportTo::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, startDialogId != 0 ? startDialogId : isDataDriven ? 4762 : 1011);
				case DialogAction::QUEST_ACCEPT:
				case DialogAction::QUEST_ACCEPT_1:
				case DialogAction::QUEST_ACCEPT_SIMPLE:
					return sendQuestStartDialog(env, workItem);
				default:
					return AbstractQuestHandler::onDialogEvent(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		if (endNpcIds.contains(targetId)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, isDataDriven ? 10002 : 2375);
				case DialogAction::SELECT_QUEST_REWARD:
					if (workItem != nullptr) {
						int64_t currentCount = player->getInventory().getItemCountByItemId(workItem->getItemId());
						if (currentCount < workItem->getCount()) {
							return sendQuestSelectionDialog(env);
						}
						removeQuestItem(env, workItem->getItemId(), currentCount, QuestStatus::COMPLETE);
					}
					qs->setQuestVar(1);
					qs->setStatus(QuestStatus::REWARD);
					updateQuestStatus(env);
					return sendQuestEndDialog(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId)) {
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
