#pragma once

#include <cstdint>

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/manager/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::manager {

/**
 * The melee auto-attack of an NPC: schedules the next swing and carries it out when the target is still there, alive, visible and in range.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class SimpleAttackManager {
private:
	/** Java: private final static class SimpleCheckedAttackAction implements Runnable (used only by the bodies, defined in the .cpp, §9.3) */
	class SimpleCheckedAttackAction;

public:
	SimpleAttackManager() = delete;

	static void performAttack(NpcAI& npcAI, int32_t delay);

private:
	static void scheduleCheckedAttackAction(NpcAI& npcAI, int32_t delay);

public:
	static bool isTargetInAttackRange(model::gameobjects::Npc& npc);

	/** Java: protected static (package access) */
	static void attackAction(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::manager
