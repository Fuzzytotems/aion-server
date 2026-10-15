#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.xml.h"

#include <vector>

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.events.OnKillEvent.
 * <p>
 * C++ notes (P4-08): `getMonsters()` returns the bound list; Java lazily stores a new empty list when none was bound. XmlQuest.register and
 * XmlQuestData.getAlternativeNpcs call getMonsters() at QuestEngine load, so after registration Java's `monster == null` test in operate is
 * dead: an event without <monster> children still runs the empty loop and `complite.operate(env)`. The port of operate (P5-06) must treat the
 * empty vector as that non-null empty list (no early return for an empty `monster`).
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
