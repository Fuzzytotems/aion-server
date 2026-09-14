#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestStatusCondition.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.conditions.QuestStatusCondition. @author Mr. Poke */
class QuestStatusCondition : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::conditions::QuestCondition {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestStatusCondition.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
