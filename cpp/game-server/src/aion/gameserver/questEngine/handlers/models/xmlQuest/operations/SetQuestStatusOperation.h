#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestStatusOperation.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.SetQuestStatusOperation. @author Mr. Poke */
class SetQuestStatusOperation : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::operations::QuestOperation {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/SetQuestStatusOperation.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
