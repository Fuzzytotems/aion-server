#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/DialogIdCondition.h"

#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

bool DialogIdCondition::doCheck(model::QuestEnv& env) const {
	switch (getOp()) {
		case model::ConditionOperation::EQUAL:
			return env.getDialogActionId() == value;
		case model::ConditionOperation::NOT_EQUAL:
			return env.getDialogActionId() != value;
		default:
			return false;
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
