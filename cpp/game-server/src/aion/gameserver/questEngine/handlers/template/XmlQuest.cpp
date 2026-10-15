#include "aion/gameserver/questEngine/handlers/template/XmlQuest.h"

#include <string>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using models::Monster;
using models::xmlQuest::events::OnKillEvent;
using models::xmlQuest::events::OnTalkEvent;

XmlQuest::XmlQuest(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, const std::vector<OnTalkEvent>& onTalkEventsValue,
	const std::vector<OnKillEvent>& onKillEventsValue)
	: AbstractTemplateQuestHandler(questIdValue), isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue)
		endNpcIds.addAll(*endNpcIdsValue);
	else
		endNpcIds.addAll(startNpcIds.snapshot());
	for (const OnTalkEvent& onTalkEvent : onTalkEventsValue)
		onTalkEvents.add(&onTalkEvent);
	for (const OnKillEvent& onKillEvent : onKillEventsValue)
		onKillEvents.add(&onKillEvent);
}

void XmlQuest::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds) {
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
		}
	}
	for (const OnTalkEvent* onTalkEvent : onTalkEvents.snapshot()) {
		for (int32_t npcId : onTalkEvent->getIds()) {
			qe.registerQuestNpc(npcId)->addOnTalkEvent(questId);
		}
	}
	for (const OnKillEvent* onKillEvent : onKillEvents.snapshot()) {
		for (const Monster& monster : onKillEvent->getMonsters()) {
			if (!monster.getNpcIds()) // Java: NullPointerException on the null npc_ids list
				throw runtime::NullPointerException("<monster> without npc_ids in quest " + std::to_string(questId));
			for (int32_t monsterId : *monster.getNpcIds()) {
				qe.registerQuestNpc(monsterId)->addOnKillEvent(questId);
			}
		}
	}
}

bool XmlQuest::onDialogEvent(model::QuestEnv& env) {
	// env.setQuestId(questId);
	for (const OnTalkEvent* onTalkEvent : onTalkEvents.snapshot()) {
		if (onTalkEvent->operate(env))
			return true;
	}

	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.contains(targetId)) {
			if (env.getDialogActionId() == DialogAction::QUEST_SELECT)
				return sendQuestDialog(env, isDataDriven ? 4762 : 1011);
			else
				return sendQuestStartDialog(env);
		}
	} else if (qs->getStatus() == QuestStatus::REWARD && endNpcIds.contains(targetId)) {
		return sendQuestEndDialog(env);
	}
	return false;
}

bool XmlQuest::onKillEvent(model::QuestEnv& env) {
	// env.setQuestId(questId);
	for (const OnKillEvent* onKillEvent : onKillEvents.snapshot()) {
		if (onKillEvent->operate(env))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
