#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.events.OnTalkEvent. @author Mr. Poke */
class OnTalkEvent : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::events::QuestEvent {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.xml.inc"
public:
	bool operate(model::QuestEnv& env) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
