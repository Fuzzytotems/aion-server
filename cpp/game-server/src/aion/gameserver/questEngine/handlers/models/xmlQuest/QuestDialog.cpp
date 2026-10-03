#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestDialog.h"

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

bool QuestDialog::operate(model::QuestEnv& env, runtime::Ptr<model::QuestState> /*qs*/) const {
	if (env.getDialogActionId() != id)
		return false;
	if (!conditions || conditions->checkConditionOfSet(env)) {
		if (operations) {
			return operations->operate(env);
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
