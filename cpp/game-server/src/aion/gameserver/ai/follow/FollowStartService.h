#pragma once

#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/ai/follow/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::follow {

/**
 * Starts the periodic follow check of a summon.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). Java's `Future<?>` return is a `runtime::FutureRef` (the caller cancels it).
 *
 * @author xTz
 */
class FollowStartService {
public:
	FollowStartService() = delete;

	/**
	 * Schedule new following checker task
	 *
	 * @param follower
	 * @param leading
	 * @return
	 */
	static runtime::FutureRef newFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading);
};

} // namespace aion::gameserver::ai::follow
