#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.QuestOperations. */
class QuestOperations : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
