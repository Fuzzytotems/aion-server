#pragma once

#include <cstdint>

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/manager/fwd.h"

namespace aion::gameserver::ai::manager {

/**
 * Drives an NPC's fight: the first attack, the next attack after each one, and what happens when the target runs away.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class AttackManager {
public:
	AttackManager() = delete;

	static void startAttacking(NpcAI& npcAI);

	static void scheduleNextAttack(NpcAI& npcAI);

	/** Java: protected static (package access) */
	static void chooseAttack(NpcAI& npcAI, int32_t delay);

	static void targetTooFar(NpcAI& npcAI);

	/**
	 * @return true if the npc should give its target up: the target ran past the map's chase_target distance, or the npc is past its
	 *         chase_home distance, or it has neither attacked nor been attacked for long enough
	 *         <p>
	 *         Java: private static. Public here so that the decision table of m5b-plan.md A-08 can call it directly; the body is unchanged
	 *         and nothing outside AttackManager calls it in the port either.
	 */
	static bool checkGiveupDistance(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::manager
