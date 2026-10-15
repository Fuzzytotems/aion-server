#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/CollectItemQuestOperation.h"

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

void CollectItemQuestOperation::doOperate(model::QuestEnv& env) const {
	// Java: removeItems == null ? true : false - an absent attribute removes the items, any value keeps them (as written)
	if (services::QuestService::collectItemCheck(env, !removeItems.has_value())) {
		if (!_true) // Java: NullPointerException on the null <true> (a required element)
			throw runtime::NullPointerException("CollectItemQuestOperation._true");
		_true->operate(env);
	} else {
		if (!_false) // Java: NullPointerException on the null <false> (a required element)
			throw runtime::NullPointerException("CollectItemQuestOperation._false");
		_false->operate(env);
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
