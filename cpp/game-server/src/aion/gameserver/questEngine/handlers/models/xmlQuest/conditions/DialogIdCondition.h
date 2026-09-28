#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/DialogIdCondition.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.conditions.DialogIdCondition. @author Mr. Poke */
class DialogIdCondition : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::conditions::QuestCondition {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/DialogIdCondition.xml.inc"
public:
	bool doCheck(model::QuestEnv& env) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
