#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperation.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.QuestOperation. @author Mr. Poke */
class QuestOperation : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperation.xml.inc"
public:
	virtual void doOperate(model::QuestEnv& env) const = 0;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
