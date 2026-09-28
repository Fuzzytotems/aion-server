#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the NOT_AT_HOME and BACK_HOME general events: the walk back to the spawn point and what an NPC does once it arrived.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class ReturningEventHandler {
public:
	ReturningEventHandler() = delete;

	/**
	 * @param npcAI
	 */
	static void onNotAtHome(NpcAI& npcAI);

	/**
	 * @param npcAI
	 */
	static void onBackHome(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
