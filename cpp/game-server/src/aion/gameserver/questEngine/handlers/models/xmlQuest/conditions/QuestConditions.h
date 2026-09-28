#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.conditions.QuestConditions. @author Mr. Poke */
class QuestConditions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
