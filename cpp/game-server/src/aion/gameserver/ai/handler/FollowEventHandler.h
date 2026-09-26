#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the FOLLOW_ME and STOP_FOLLOW_ME creature events of an NPC and the range check of a following NPC.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). `isInRange`'s object is a `Ptr` because Java checks it for null
 * (FollowEventHandler.java:51) and NpcAI::isDestinationReached passes the owner's target into it.
 *
 * @author ATracer
 */
class FollowEventHandler {
public:
	FollowEventHandler() = delete;

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void follow(NpcAI& npcAI, model::gameobjects::Creature& creature);

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void creatureMoved(NpcAI& npcAI, model::gameobjects::Creature& creature);

	/**
	 * @param creature
	 */
	static void checkFollowTarget(NpcAI& npcAI, model::gameobjects::Creature& creature);

	/** ai: the erased `AbstractAI<? extends Creature>` (hub-headers.md §8.1) */
	static bool isInRange(AbstractAI& ai, runtime::Ptr<model::gameobjects::VisibleObject> object);

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void stopFollow(NpcAI& npcAI, model::gameobjects::Creature& creature);
};

} // namespace aion::gameserver::ai::handler
