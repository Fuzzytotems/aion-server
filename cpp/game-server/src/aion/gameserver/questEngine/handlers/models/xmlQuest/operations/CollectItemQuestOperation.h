#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/CollectItemQuestOperation.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.operations.CollectItemQuestOperation. @author Mr. Poke */
class CollectItemQuestOperation : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::operations::QuestOperation {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/CollectItemQuestOperation.xml.inc"
public:
	void doOperate(model::QuestEnv& env) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
