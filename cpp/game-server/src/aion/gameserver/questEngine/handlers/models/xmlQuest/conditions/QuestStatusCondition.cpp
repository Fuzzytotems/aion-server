#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestStatusCondition.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

namespace {

/** Java QuestStatus.value(): START 3, REWARD 4, COMPLETE 5, LOCKED 6 (QuestStatus.java:11-14), the ordinal + 3 */
constexpr int32_t statusValue(model::QuestStatus status) noexcept {
	return static_cast<int32_t>(status) + 3;
}

} // namespace

bool QuestStatusCondition::doCheck(model::QuestEnv& env) const {
	runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
	int32_t qstatus = 0;
	int32_t id = env.getQuestId();
	if (questId)
		id = *questId;
	runtime::Ptr<model::QuestState> qs = player->getQuestStateList()->getQuestState(id);
	if (qs)
		qstatus = statusValue(qs->getStatus());
	switch (getOp()) {
		case model::ConditionOperation::EQUAL:
			return qstatus == statusValue(value);
		case model::ConditionOperation::GREATER:
			return qstatus > statusValue(value);
		case model::ConditionOperation::GREATER_EQUAL:
			return qstatus >= statusValue(value);
		case model::ConditionOperation::LESSER:
			return qstatus < statusValue(value);
		case model::ConditionOperation::LESSER_EQUAL:
			return qstatus <= statusValue(value);
		case model::ConditionOperation::NOT_EQUAL:
			return qstatus != statusValue(value);
		default:
			return false;
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
