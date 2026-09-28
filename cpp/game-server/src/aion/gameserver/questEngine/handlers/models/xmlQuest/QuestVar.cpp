#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

bool QuestVar::operate(model::QuestEnv& /*env*/, runtime::Ptr<model::QuestState> /*qs*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
