#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the DIED general event of an NPC: the death shout, AIState::DIED and the target cleared.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class DiedEventHandler {
public:
	DiedEventHandler() = delete;

	static void onDie(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
