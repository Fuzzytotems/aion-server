#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles what an NPC does when it reached, lost or changed its target.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class TargetEventHandler {
public:
	TargetEventHandler() = delete;

	/**
	 * @param npcAI
	 */
	static void onTargetReached(NpcAI& npcAI);

	/**
	 * @param npcAI
	 */
	static void onTargetTooFar(NpcAI& npcAI);

	/**
	 * @param npcAI
	 */
	static void onTargetGiveup(NpcAI& npcAI);

	/**
	 * @param npcAI
	 * @param creature
	 */
	static void onTargetChange(NpcAI& npcAI, model::gameobjects::Creature& creature);

private:
	static void checkAggro(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::handler
