#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/QuestEvent.xml.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.events.QuestEvent.
 * <p>
 * C++ notes (P4-08): `getIds()` returns the bound ids or an empty list where Java lazily stores a new empty list into the template (the
 * templates are const after load; no Java caller adds to the returned list).
 *
 * @author Mr. Poke
 */
class QuestEvent : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/QuestEvent.xml.inc"
public:
	const std::vector<int32_t>& getIds() const;

	virtual bool operate(model::QuestEnv& env) const;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
