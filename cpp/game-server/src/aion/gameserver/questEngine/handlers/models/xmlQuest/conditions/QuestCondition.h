#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestCondition.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.conditions.QuestCondition. @author Mr. Poke */
class QuestCondition : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestCondition.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
