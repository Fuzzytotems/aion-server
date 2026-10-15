#include "aion/gameserver/questEngine/handlers/template/ReportToMany.h"

#include <algorithm>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"
#include "aion/gameserver/questEngine/handlers/models/NpcInfos.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::quest::QuestItems;
using model::QuestState;
using model::QuestStatus;
using models::NpcInfos;

// Java: LoggerFactory.getLogger(ReportToMany.class) at each of its two warnings
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.ReportToMany");

namespace {

/** Java `npcInfo.getNpcIds()` dereferenced (npc_ids is required; NullPointerException without it) */
const std::vector<int32_t>& npcIdsOf(const NpcInfos& npcInfo) {
	if (!npcInfo.getNpcIds())
		throw runtime::NullPointerException("NpcInfos.npcIds");
	return *npcInfo.getNpcIds();
}

bool containsId(const std::vector<int32_t>& ids, int32_t id) {
	return std::ranges::find(ids, id) != ids.end();
}

} // namespace

ReportToMany::ReportToMany(int32_t questIdValue, int32_t startItemIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::vector<NpcInfos>& npcInfosValue, int32_t startDialogIdValue, bool missionValue)
	: AbstractTemplateQuestHandler(questIdValue), startItemId(startItemIdValue), startDialogId(startDialogIdValue), mission(missionValue),
	  isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	for (const NpcInfos& npcInfo : npcInfosValue)
		npcInfos.add(&npcInfo);
	runtime::Ptr<runtime::RcArrayList<const QuestItems*>> items = workItems.get();
	if (items && items->size() > npcInfos.size())
		log.warn("Q{} has more work items than quest steps", questId);
}

void ReportToMany::register_() {
	if (mission) {
		qe.registerOnLevelChanged(questId);
	}
	if (startItemId != 0)
		qe.registerQuestItem(startItemId, questId);
	else {
		for (int32_t startNpcId : startNpcIds) {
			qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
			qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
		}
	}
	for (const NpcInfos* npcInfo : npcInfos.snapshot()) {
		for (int32_t npcId : npcIdsOf(*npcInfo)) {
			qe.registerQuestNpc(npcId)->addOnTalkEvent(questId);
		}
	}
}

bool ReportToMany::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if ((startNpcIds.isEmpty() || startNpcIds.contains(targetId)) &&
			(startItemId == 0 || player->getInventory().getFirstItemByItemId(startItemId) != nullptr)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_ACCEPT:
				case DialogAction::QUEST_ACCEPT_1:
				case DialogAction::QUEST_ACCEPT_SIMPLE:
					return sendQuestStartDialog(env);
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, startDialogId != 0 ? startDialogId : isDataDriven ? 4762 : 1011);
				default:
					return AbstractQuestHandler::onDialogEvent(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		int32_t step = qs->getQuestVarById(0); // starting from 0
		if (step > getMaxStep()) {
			log.warn("Missing NpcInfo for quest {} step #{}", questId, step + 1);
			return false;
		}
		const NpcInfos* targetNpcInfo = npcInfos.get(step);
		if (!containsId(npcIdsOf(*targetNpcInfo), targetId))
			return false;

		switch (dialogActionId) {
			case DialogAction::QUEST_SELECT:
				return sendQuestDialog(env, getDialogId(step));
			case DialogAction::SETPRO1:
			case DialogAction::SETPRO2:
			case DialogAction::SETPRO3:
			case DialogAction::SETPRO4:
			case DialogAction::SETPRO5:
			case DialogAction::SETPRO6:
			case DialogAction::SETPRO7:
			case DialogAction::SETPRO8:
			case DialogAction::SETPRO9:
			case DialogAction::SETPRO10:
			case DialogAction::SETPRO11:
			case DialogAction::SETPRO12: {
				changeQuestStep(env, step, step + 1);
				runtime::Ptr<runtime::RcArrayList<const QuestItems*>> items = workItems.get();
				if (items && items->size() > step)
					giveQuestItem(env, items->get(step)->getItemId(), items->get(step)->getCount());
				return closeDialogWindow(env);
			}
			case DialogAction::SET_SUCCEED:
			case DialogAction::SELECT_QUEST_REWARD:
			case DialogAction::CHECK_USER_HAS_QUEST_ITEM:
			case DialogAction::CHECK_USER_HAS_QUEST_ITEM_SIMPLE:
				if (dialogActionId == DialogAction::SET_SUCCEED) { // set reward from pre-end npc (end npc is another one who will then give the reward)
					rewardStatusFromRewardNpc.set(false);
					step++;
				}
				if (step < getMaxStep() || !validateAndRemoveItems(env))
					return sendQuestSelectionDialog(env);
				qs->setQuestVarById(0, step);
				qs->setStatus(QuestStatus::REWARD);
				updateQuestStatus(env);
				return sendQuestEndDialog(env);
			default:
				if (targetNpcInfo->getMovie() != 0)
					playQuestMovie(env, targetNpcInfo->getMovie());
				return AbstractQuestHandler::onDialogEvent(env);
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		const NpcInfos* endNpcInfo = npcInfos.get(getMaxStep());
		if (!containsId(npcIdsOf(*endNpcInfo), targetId))
			return false;
		// if talking to an end npc who did not set the reward state himself: show full reward dialog instead of only last page (otherwise it's
		// never readable)
		if (dialogActionId == DialogAction::USE_OBJECT && !rewardStatusFromRewardNpc.get())
			return sendQuestDialog(env, isDataDriven ? 10002 : 2375);
		return sendQuestEndDialog(env);
	}
	return false;
}

int32_t ReportToMany::getMaxStep() const {
	return npcInfos.size() - 1;
}

int32_t ReportToMany::getDialogId(int32_t var) const {
	if (var == getMaxStep())
		return isDataDriven ? 10002 : 2375;
	else
		return (isDataDriven ? 1011 : 1352) + var * 341;
}

bool ReportToMany::validateAndRemoveItems(model::QuestEnv& env) {
	if (!services::QuestService::collectItemCheck(env, true))
		return false;
	if (startItemId != 0 && !removeQuestItem(env, startItemId, 1))
		return false;
	if (runtime::Ptr<runtime::RcArrayList<const QuestItems*>> items = workItems.get()) {
		for (const QuestItems* workItem : items->snapshot())
			removeQuestItem(env, workItem->getItemId(), workItem->getCount(), QuestStatus::COMPLETE);
	}
	return true;
}

HandlerResult ReportToMany::onItemUseEvent(model::QuestEnv& env, gameserver::model::gameobjects::Item& /*item*/) {
	if (startItemId != 0) {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (!qs || qs->isStartable()) {
			return fromBoolean(sendQuestDialog(env, 4));
		}
	}
	return HandlerResult::UNKNOWN;
}

void ReportToMany::onLevelChangedEvent(Player& player) {
	defaultOnLevelChangedEvent(player);
}

} // namespace aion::gameserver::questEngine::handlers::template_
