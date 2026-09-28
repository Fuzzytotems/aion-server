#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the ACTIVATE and DEACTIVATE general events of an NPC: the map region its npc stands in became active or inactive.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class ActivateEventHandler {
public:
	ActivateEventHandler() = delete;

	static void onActivate(NpcAI& npcAI);

	static void onDeactivate(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
