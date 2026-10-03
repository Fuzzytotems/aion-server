#include "aion/gameserver/questEngine/task/QuestTasks.h"

#include <optional>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/task/FollowingNpcCheckTask.h"
#include "aion/gameserver/questEngine/task/checker/CoordinateDestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/TargetDestinationChecker.h"
#include "aion/gameserver/questEngine/task/checker/ZoneChecker.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::questEngine::task {

namespace {

using gameserver::model::gameobjects::Npc;
using gameserver::model::templates::spawns::SpawnSearchResult;

/**
 * Java: ThreadPoolManager.getInstance().scheduleAtFixedRate(new FollowingNpcCheckTask(...), 1000, 1000). Java schedules the Runnable itself;
 * the Ref inside the closure retains the task and, through its fields, the env (the player) and the checker (the npc and its target), as
 * Java's scheduled Runnable does, until the task is cancelled. The closure is pinned to the following npc as well, so the leak census can
 * attribute it to an owner (FollowStartService.cpp has the same note for the summons' follow task); the pin retains nothing the task does not.
 */
runtime::FutureRef scheduleCheck(Npc& npc, runtime::Ref<FollowingNpcCheckTask> task) {
	return utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&npc}, [task] { task->run(); }, 1000, 1000);
}

} // namespace

runtime::FutureRef QuestTasks::newFollowingToTargetCheckTask(model::QuestEnv& env, Npc& npc, Npc& target) {
	return scheduleCheck(npc, FollowingNpcCheckTask::create(env, *checker::TargetDestinationChecker::create(npc, target)));
}

runtime::FutureRef QuestTasks::newFollowingToTargetCheckTask(model::QuestEnv& env, Npc& npc, int32_t npcTargetId) {
	std::optional<SpawnSearchResult> searchResult = dataholders::DataManager::SPAWNS_DATA->getFirstSpawnByNpcId(npc.getWorldId(), npcTargetId);
	if (!searchResult) {
		throw runtime::IllegalArgumentException("Supplied npc doesn't exist: " + std::to_string(npcTargetId));
	}
	return scheduleCheck(npc, FollowingNpcCheckTask::create(env, *checker::CoordinateDestinationChecker::create(npc, searchResult->getSpot().getX(),
		searchResult->getSpot().getY(), searchResult->getSpot().getZ())));
}

runtime::FutureRef QuestTasks::newFollowingToTargetCheckTask(model::QuestEnv& env, Npc& npc, float x, float y, float z) {
	return scheduleCheck(npc, FollowingNpcCheckTask::create(env, *checker::CoordinateDestinationChecker::create(npc, x, y, z)));
}

runtime::FutureRef QuestTasks::newFollowingToTargetCheckTask(model::QuestEnv& env, Npc& npc, const world::zone::ZoneName* zoneName) {
	return scheduleCheck(npc, FollowingNpcCheckTask::create(env, *checker::ZoneChecker::create(npc, zoneName)));
}

} // namespace aion::gameserver::questEngine::task
