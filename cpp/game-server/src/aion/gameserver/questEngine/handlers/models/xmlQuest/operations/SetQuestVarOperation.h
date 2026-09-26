#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestVarOperation.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.SetQuestVarOperation. @author Mr. Poke */
class SetQuestVarOperation : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::operations::QuestOperation {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestVarOperation.xml.inc"
public:
	void doOperate(model::QuestEnv& env) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
