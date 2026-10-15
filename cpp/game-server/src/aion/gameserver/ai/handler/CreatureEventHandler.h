#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Decides whether an NPC aggroes a creature it sees or that moved near it: the predicate chain of CreatureEventHandler.java:56-110.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class CreatureEventHandler {
public:
	CreatureEventHandler() = delete;

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void onCreatureMoved(NpcAI& npcAI, model::gameobjects::Creature& creature);

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void onCreatureSee(NpcAI& npcAI, model::gameobjects::Creature& creature);

	/** Java: protected static (package access; TargetEventHandler.checkAggro calls it) */
	static void checkAggro(NpcAI& ai, model::gameobjects::Creature& creature);

private:
	static bool isInSeeRange(model::gameobjects::Creature& creature, model::gameobjects::Npc& npc);

	static bool validateAggro(model::gameobjects::Npc& owner, model::gameobjects::Creature& creature);
};

} // namespace aion::gameserver::ai::handler
