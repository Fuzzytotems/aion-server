#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the BEFORE_SPAWNED, SPAWNED and DESPAWNED general events of an NPC.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class SpawnEventHandler {
public:
	SpawnEventHandler() = delete;

	static void onSpawn(NpcAI& npcAI);

	static void onDespawn(NpcAI& npcAI);

	static void onBeforeSpawn(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
