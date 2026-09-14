#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/QuestEvent.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.events.QuestEvent. @author Mr. Poke */
class QuestEvent : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/QuestEvent.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
