#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.xml.h"

#include <vector>

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.events.OnKillEvent.
 * <p>
 * C++ notes (P4-08): `getMonsters()` returns the bound list, empty without <monster> children (Java: List.of() for the null field). JAXB
 * leaves the field null exactly when there are no <monster> children, so the empty vector is Java's `monster == null` in operate. Upstream
 * 15eaf77b6 stopped getMonsters() from storing an empty list, so that test is live again: an event without <monster> does nothing.
 *
 * @author Mr. Poke, Bobobear, Pad
 */
class OnKillEvent : public ::aion::gameserver::questEngine::handlers::models::xmlQuest::events::QuestEvent {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.xml.inc"
public:
	const std::vector<Monster>& getMonsters() const { return monster; }

	bool operate(model::QuestEnv& env) const override;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
