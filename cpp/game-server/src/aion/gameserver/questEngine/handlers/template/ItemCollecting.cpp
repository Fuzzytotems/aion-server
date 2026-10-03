#include "aion/gameserver/questEngine/handlers/template/ItemCollecting.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestCategory.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.ItemCollecting");

ItemCollecting::ItemCollecting(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue, int32_t nextNpcIdValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, std::string startZoneValue, int32_t questMovieValue, int32_t startDialogIdValue,
	int32_t startDialogId2Value, int32_t checkOkDialogIdValue, int32_t checkFailDialogIdValue)
	: AbstractTemplateQuestHandler(questIdValue), questMovie(questMovieValue), nextNpcId(nextNpcIdValue), startDialogId(startDialogIdValue),
	  startDialogId2(startDialogId2Value), checkOkDialogId(checkOkDialogIdValue), checkFailDialogId(checkFailDialogIdValue),
	  isDataDriven(questTemplateOf(questIdValue).isDataDriven()), startZone(std::move(startZoneValue)) {
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

void ItemCollecting::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (nextNpcId != 0) {
		qe.registerQuestNpc(nextNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds)
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
	}
	if (runtime::Ptr<runtime::RcHashSet<int32_t>> items = actionItems.get()) {
		for (int32_t actionItem : items->snapshot()) {
			qe.registerQuestNpc(actionItem)->addOnTalkEvent(questId);
			qe.registerCanAct(questId, actionItem);
		}
	}
	// Java: startZone != null ("" here) and ZoneName.get(startZone) twice, as written (each call warns for a missing zone)
	if (!startZone.empty() && !commons::utils::StringUtils::equalsIgnoreCase(world::zone::ZoneName::get(startZone)->name(), "NONE"))
		qe.registerOnEnterZone(world::zone::ZoneName::get(startZone), questId);
}

bool ItemCollecting::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId) ||
			questTemplateOf(questId).getCategory() == gameserver::model::templates::quest::QuestCategory::FACTION) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, startDialogId != 0 ? startDialogId : isDataDriven ? 4762 : 1011);
				case DialogAction::SETPRO1:
					services::QuestService::startQuest(env);
					return closeDialogWindow(env);
				case DialogAction::SELECT1_1:
					if (questMovie != 0) {
						playQuestMovie(env, questMovie);
					}
					return sendQuestDialog(env, 1012);
				case DialogAction::QUEST_ACCEPT:
				case DialogAction::QUEST_ACCEPT_1:
				case DialogAction::QUEST_ACCEPT_SIMPLE:
					return sendQuestStartDialog(env, workItem);
				default:
					return AbstractQuestHandler::onDialogEvent(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		int32_t var = qs->getQuestVarById(0);
		if (targetId == nextNpcId && var == 0) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, 1352);
				case DialogAction::SETPRO1:
					return defaultCloseDialog(env, 0, 1);
			}
		} else if (endNpcIds.contains(targetId)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, startDialogId2 != 0 ? startDialogId2 : isDataDriven ? 1011 : 2375);
				case DialogAction::CHECK_USER_HAS_QUEST_ITEM: {
					int32_t okDialogId = checkOkDialogId != 0 ? checkOkDialogId : isDataDriven ? 10000 : 5;
					int32_t failDialogId = checkFailDialogId != 0 ? checkFailDialogId : isDataDriven ? 10001 : 2716;
					return checkQuestItems(env, var, var, true, okDialogId, failDialogId); // reward
				}
				case DialogAction::CHECK_USER_HAS_QUEST_ITEM_SIMPLE:
					return checkQuestItemsSimple(env, var, var, true, 5, 0, 0); // reward
				case DialogAction::FINISH_DIALOG:
					return sendQuestSelectionDialog(env);
				case DialogAction::SET_SUCCEED:
					qs->setStatus(QuestStatus::REWARD);
					updateQuestStatus(env);
					return closeDialogWindow(env);
				case DialogAction::SETPRO1:
					return checkQuestItemsSimple(env, var, var, true, 5, 0, 0);
				case DialogAction::SETPRO2:
					return checkQuestItemsSimple(env, var, var, true, 6, 0, 0);
				case DialogAction::SETPRO3:
					return checkQuestItemsSimple(env, var, var, true, 7, 0, 0);
				case DialogAction::SETPRO4:
					return checkQuestItemsSimple(env, var, var, true, 8, 0, 0);
			}
		} else if (runtime::Ptr<runtime::RcHashSet<int32_t>> items = actionItems.get(); items && items->contains(targetId)) {
			return true; // looting
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId)) {
			if (workItem != nullptr) {
				int64_t currentCount = player->getInventory().getItemCountByItemId(workItem->getItemId());
				if (currentCount > 0)
					removeQuestItem(env, workItem->getItemId(), currentCount, QuestStatus::COMPLETE);
			}
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool ItemCollecting::onEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* zoneName) {
	if (zoneName == nullptr)
		throw runtime::NullPointerException("zoneName");
	// Java: zoneName.name().equalsIgnoreCase(startZone) is false for a null startZone ("" here)
	if (!startZone.empty() && commons::utils::StringUtils::equalsIgnoreCase(zoneName->name(), startZone)) {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (!qs || qs->isStartable()) {
			services::QuestService::startQuest(env);
			return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
