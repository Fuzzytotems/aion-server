#include "aion/gameserver/questEngine/handlers/template/ReportOnLevelUp.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::questEngine::handlers::template_ {

using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;

ReportOnLevelUp::ReportOnLevelUp(int32_t questIdValue, const std::optional<std::vector<int32_t>>& endNpcIdsValue)
	: AbstractTemplateQuestHandler(questIdValue) {
	if (endNpcIdsValue) {
		endNpcIds.addAll(*endNpcIdsValue);
	}
}

void ReportOnLevelUp::register_() {
	for (int32_t endNpcId : endNpcIds)
		qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);

	qe.registerOnEnterWorld(questId);
	qe.registerOnLevelChanged(questId);
}

bool ReportOnLevelUp::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t targetId = env.getTargetId();

	if (!qs)
		return false;
	if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId))
			return sendQuestEndDialog(env);
	}
	return false;
}

bool ReportOnLevelUp::onEnterWorldEvent(model::QuestEnv& env) {
	return startQuest(*env.getPlayer());
}

void ReportOnLevelUp::onLevelChangedEvent(Player& player) {
	startQuest(player);
}

bool ReportOnLevelUp::startQuest(Player& player) {
	if (!player.getQuestStateList()->hasQuest(questId)) {
		runtime::Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, player, questId);
		return services::QuestService::startQuest(*env, QuestStatus::REWARD, false);
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
