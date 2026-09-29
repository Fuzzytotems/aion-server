#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperation.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

bool QuestOperations::operate(model::QuestEnv& env) const {
	// Java: if (operations != null) - an absent list is the bound empty vector
	for (const std::unique_ptr<QuestOperation>& oper : operations) {
		oper->doOperate(env);
	}
	return isOverride();
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
