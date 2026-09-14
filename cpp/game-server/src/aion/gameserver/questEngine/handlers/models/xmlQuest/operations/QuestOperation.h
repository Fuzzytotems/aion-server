#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperation.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.QuestOperation. @author Mr. Poke */
class QuestOperation : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperation.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
