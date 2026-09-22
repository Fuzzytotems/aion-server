#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/manager/fwd.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/walker/fwd.h"

namespace aion::gameserver::ai::manager {

/**
 * Starts, steps and stops the walking of an NPC: the random walk around its spawn and the route walk of a path walker.
 * <p>
 * A static-only utility class (hub-headers.md §11.1) with one public static field (Java declares it non-final; nothing writes it).
 *
 * @author ATracer
 */
class WalkManager {
public:
	static inline runtime::Field<int8_t> RANDOM_WALK_GEO_FLAGS{static_cast<int8_t>(
		geoEngine::collision::getId(geoEngine::collision::CollisionIntention::CANT_SEE_COLLISIONS) |
		geoEngine::collision::getId(geoEngine::collision::CollisionIntention::WALK) |
		geoEngine::collision::getId(geoEngine::collision::CollisionIntention::PHYSICAL_SEE_THROUGH))};

	WalkManager() = delete;

	/**
	 * @return True, if the npc started walking. False if walking is disabled, not supported, or the npc is already walking.
	 */
	static bool startWalking(NpcAI& npcAI);

private:
	static bool startRandomWalking(NpcAI& npcAI);

	static bool startRouteWalking(NpcAI& npcAI);

public:
	static void startForcedWalking(NpcAI& npcAI, float x, float y, float z);

	/** Java: protected static (package access) */
	static const model::templates::walker::RouteStep* findNextRoutStep(model::gameobjects::Npc& owner);

	/** Java: protected static (package access) */
	static const model::templates::walker::RouteStep* findClosestRouteStep(model::gameobjects::Npc& owner);

	/** Java: protected static (package access) */
	static const model::templates::walker::RouteStep* findNextRouteStepAfterPause(model::gameobjects::Npc& owner,
		const model::templates::walker::RouteStep& currentStep);

	static void targetReached(NpcAI& npcAI);

	/** Java: protected static (package access) */
	static void chooseNextRouteStep(NpcAI& npcAI);

private:
	static void chooseNextRandomPoint(NpcAI& npcAI);

public:
	static void stopWalking(NpcAI& npcAI);

	static bool isArrivedAtPoint(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::manager
