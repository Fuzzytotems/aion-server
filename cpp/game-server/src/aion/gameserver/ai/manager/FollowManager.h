#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/manager/fwd.h"

namespace aion::gameserver::ai::manager {

/**
 * Moves a following NPC back towards its target when the target walked out of range.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class FollowManager {
public:
	FollowManager() = delete;

	static void targetTooFar(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::manager
