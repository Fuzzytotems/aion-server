#include "aion/gameserver/questEngine/handlers/template/KillInWorld.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/VortexService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.handlers.template.KillInWorld");

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

KillInWorld::KillInWorld(int32_t questIdValue, const std::optional<std::vector<int32_t>>& endNpcIdsValue,
	const std::optional<std::vector<int32_t>>& startNpcIdsValue, const std::optional<std::vector<int32_t>>& worldIdsValue, int32_t killAmountValue,
	int32_t minRankValue, int32_t levelDiffValue, int32_t invasionWorld, int32_t startDialogIdValue, int32_t startDistanceNpcIdValue,
	int32_t endDialogIdValue)
	: AbstractTemplateQuestHandler(questIdValue), killAmount(killAmountValue == 0 ? 1 : killAmountValue), minRank(minRankValue),
	  levelDiff(levelDiffValue), invasionWorldId(invasionWorld), startDialogId(startDialogIdValue), startDistanceNpcId(startDistanceNpcIdValue),
	  endDialogId(endDialogIdValue), isDataDriven(questTemplateOf(questIdValue).isDataDriven()) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue)
		endNpcIds.addAll(*endNpcIdsValue);
	else
		endNpcIds.addAll(startNpcIds.snapshot());
	if (worldIdsValue) {
		worldIds.addAll(*worldIdsValue);
	} else {
		for (const gameserver::model::templates::world::WorldMapTemplate* template_ : *dataholders::DataManager::WORLD_MAPS_DATA)
			worldIds.add(template_->getMapId());
	}
	if (workItems.get())
		log.warn("Q{} should not have work items", questId);
}

void KillInWorld::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds)
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
	}
	for (int32_t worldId : worldIds)
		qe.registerOnKillInWorld(worldId, questId);

	if (invasionWorldId != 0)
		qe.registerOnEnterWorld(questId);

	if (startDistanceNpcId != 0)
		qe.registerQuestNpc(startDistanceNpcId, 300)->addOnAtDistanceEvent(questId);
}

bool KillInWorld::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t targetId = env.getTargetId();
	int32_t dialogActionId = env.getDialogActionId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId)) {
			switch (dialogActionId) {
				case DialogAction::QUEST_SELECT:
					return sendQuestDialog(env, startDialogId != 0 ? startDialogId : isDataDriven ? 4762 : 1011);
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
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, endDialogId != 0 ? endDialogId : isDataDriven ? 10002 : 2375);
			}
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool KillInWorld::onEnterWorldEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	runtime::Ptr<gameserver::model::vortex::VortexLocation> vortexLoc = services::VortexService::getInstance().getLocationByWorld(invasionWorldId);
	if (player->getWorldId() == invasionWorldId) {
		if (!qs || qs->isStartable()) {
			if ((vortexLoc && vortexLoc->isActive()) || searchOpenRift())
				return services::QuestService::startQuest(env);
		}
	}
	return false;
}

bool KillInWorld::searchOpenRift() {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<gameserver::model::rift::RiftLocation>>> locations =
		services::RiftService::getInstance().getRiftLocations();
	if (!locations) // Java: RiftService.locations is null before initRiftLocations
		throw runtime::NullPointerException("RiftService.getRiftLocations()");
	for (const runtime::Ptr<gameserver::model::rift::RiftLocation>& loc : locations->values()) {
		if (loc->getWorldId() == invasionWorldId && loc->isOpened()) {
			return true;
		}
	}
	return false;
}

bool KillInWorld::onKillInWorldEvent(model::QuestEnv& env) {
	// Rank restriction
	if (minRank > 0 && gameserver::model::gameobjects::detail::abyssRankId(killedPlayerOf(env)->getAbyssRank()->getRank()) < minRank)
		return false;
	// Level restriction
	if (levelDiff > 0 && (env.getPlayer()->getLevel() - killedPlayerOf(env)->getLevel()) > levelDiff)
		return false;
	return defaultOnKillRankedEvent(env, 0, killAmount, true, isDataDriven); // reward
}

bool KillInWorld::onAtDistanceEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (!qs || qs->isStartable())
		return services::QuestService::startQuest(env);
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
