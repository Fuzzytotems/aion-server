#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.events.OnKillEvent. @author Mr. Poke, Bobobear, Pad */
class OnKillEvent : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::events::QuestEvent {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
