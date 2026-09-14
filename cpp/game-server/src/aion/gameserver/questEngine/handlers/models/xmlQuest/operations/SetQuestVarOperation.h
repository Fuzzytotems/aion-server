#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestVarOperation.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.SetQuestVarOperation. @author Mr. Poke */
class SetQuestVarOperation : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::operations::QuestOperation {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestVarOperation.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
