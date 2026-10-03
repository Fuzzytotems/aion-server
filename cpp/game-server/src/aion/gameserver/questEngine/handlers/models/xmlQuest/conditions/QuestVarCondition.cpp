#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestVarCondition.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

bool QuestVarCondition::doCheck(model::QuestEnv& env) const {
	runtime::Ptr<model::QuestState> qs = env.getPlayer()->getQuestStateList()->getQuestState(env.getQuestId());
	if (!qs) {
		return false;
	}
	int32_t var = qs->getQuestVars()->getVarById(varId);
	switch (getOp()) {
		case model::ConditionOperation::EQUAL:
			return var == value;
		case model::ConditionOperation::GREATER:
			return var > value;
		case model::ConditionOperation::GREATER_EQUAL:
			return var >= value;
		case model::ConditionOperation::LESSER:
			return var < value;
		case model::ConditionOperation::LESSER_EQUAL:
			return var <= value;
		case model::ConditionOperation::NOT_EQUAL:
			return var != value;
		default:
			return false;
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
