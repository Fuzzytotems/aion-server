#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestCondition.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.conditions.QuestCondition. @author Mr. Poke */
class QuestCondition : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestCondition.xml.inc"
public:
	virtual bool doCheck(model::QuestEnv& env) const = 0;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
