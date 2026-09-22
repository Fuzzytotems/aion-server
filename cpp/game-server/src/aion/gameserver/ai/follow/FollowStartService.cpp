#include "aion/gameserver/ai/follow/FollowStartService.h"

#include "aion/gameserver/ai/follow/FollowSummonTaskAI.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::ai::follow {

runtime::FutureRef FollowStartService::newFollowingToTargetCheckTask(model::gameobjects::Summon& follower, model::gameobjects::Creature& leading) {
	// Java schedules the Runnable itself; the Ref inside the task closure retains the task and, through its fields, the summon, its master and
	// the target (as Java's scheduled Runnable does). The task is pinned to the summon as well, so the leak census can attribute it to its owner
	// (LifeStatsRestoreService.cpp has the same note); the pin adds no retention Java does not have.
	runtime::Ref<FollowSummonTaskAI> task = FollowSummonTaskAI::create(leading, follower);
	return utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&follower}, [task] { task->run(); }, 1000, 1000);
}

} // namespace aion::gameserver::ai::follow
