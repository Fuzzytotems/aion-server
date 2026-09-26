#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestStatusOperation.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.SetQuestStatusOperation. @author Mr. Poke */
class SetQuestStatusOperation : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::operations::QuestOperation {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestStatusOperation.xml.inc"
public:
	void doOperate(model::QuestEnv& env) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
