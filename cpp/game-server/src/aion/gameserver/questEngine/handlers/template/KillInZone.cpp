#include "aion/gameserver/questEngine/handlers/template/KillInZone.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.KillInZone");

namespace {

/** Java `(Player) env.getVisibleObject()` dereferenced: NullPointerException without a target, ClassCastException for another object */
runtime::Ptr<Player> killedPlayerOf(model::QuestEnv& env) {
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> object = env.getVisibleObject();
	if (!object)
		throw runtime::NullPointerException("env.getVisibleObject()");
	runtime::Ptr<Player> victim = runtime::as<Player>(object);
	if (!victim)
		throw runtime::ClassCastException("the killed object cannot be cast to class com.aionemu.gameserver.model.gameobjects.player.Player");
	return victim;
}

} // namespace

KillInZone::KillInZone(int32_t questIdValue, const std::optional<std::vector<int32_t>>& endNpcIdsValue,
	const std::optional<std::vector<int32_t>>& startNpcIdsValue, const std::optional<std::vector<std::string>>& zonesValue, int32_t killAmountValue,
	int32_t minRankValue, int32_t levelDiffValue, int32_t startDistanceNpcValue)
	: AbstractTemplateQuestHandler(questIdValue), killAmount(killAmountValue == 0 ? 1 : killAmountValue), minRank(minRankValue),
	  levelDiff(levelDiffValue), startDistanceNpc(startDistanceNpcValue), isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue) {
		endNpcIds.addAll(*endNpcIdsValue);
	} else {
		// Java: this.endNpcIds.addAll(startNpcIds) - the parameter, not the field: NullPointerException when both lists are null
		if (!startNpcIdsValue)
			throw runtime::NullPointerException("KillInZone " + std::to_string(questIdValue) + ": startNpcIds");
		endNpcIds.addAll(*startNpcIdsValue);
	}
	if (zonesValue) {
		zones.addAll(*zonesValue);
	} else {
		for (const gameserver::model::templates::zone::ZoneTemplate& template_ : dataholders::DataManager::ZONE_DATA->zoneList)
			zones.add(template_.getXmlName());
	}
	if (workItems.get())
		log.warn("Q{} should not have work items", questId);
}

void KillInZone::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds)
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
	}
	for (const std::string& zone : zones)
		qe.registerOnKillInZone(zone, questId);
	if (startDistanceNpc != 0)
		qe.registerQuestNpc(startDistanceNpc, 300)->addOnAtDistanceEvent(questId);
}

bool KillInZone::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t targetId = env.getTargetId();
	int32_t dialogActionId = env.getDialogActionId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, isDataDriven ? 4762 : 1011);
				case DialogAction::QUEST_ACCEPT:
				case DialogAction::QUEST_ACCEPT_1:
				case DialogAction::QUEST_ACCEPT_SIMPLE:
					return sendQuestStartDialog(env);
				default:
					return AbstractQuestHandler::onDialogEvent(env);
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId)) {
			if (isDataDriven && dialogActionId == DialogAction::USE_OBJECT)
				return sendQuestDialog(env, 10002);
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool KillInZone::onKillInZoneEvent(model::QuestEnv& env) {
	// Rank restriction
	if (minRank > 0 && gameserver::model::gameobjects::detail::abyssRankId(killedPlayerOf(env)->getAbyssRank()->getRank()) < minRank)
		return false;
	// Level restriction
	if (levelDiff > 0 && (env.getPlayer()->getLevel() - killedPlayerOf(env)->getLevel()) > levelDiff)
		return false;
	return defaultOnKillInZoneEvent(env, 0, killAmount, true, isDataDriven); // reward
}

bool KillInZone::onAtDistanceEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->isStartable()) {
		services::QuestService::startQuest(env);
		return true;
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
