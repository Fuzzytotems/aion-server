#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/questEngine/task/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::questEngine::task {

/**
 * Schedules the check of a quest escort (FollowingNpcCheckTask) with a destination checker: a creature, the first spawn spot of an npc id, a
 * point, a zone. The first check runs a second after the call, then one a second.
 * <p>
 * A static-only utility class (hub-headers.md §11.1; fieldmap K5). Java's `Future<?>` return is a `runtime::FutureRef`: the caller keeps it
 * (AbstractQuestHandler.defaultStartFollowEvent adds it as the player's QUEST_FOLLOW task) and the task cancels it through that task id.
 *
 * @author ATracer
 */
class QuestTasks {
public:
	QuestTasks() = delete;

	/**
	 * Schedule new following checker task
	 *
	 * @param player
	 * @param npc
	 * @param target
	 * @return
	 */
	static runtime::FutureRef newFollowingToTargetCheckTask(model::QuestEnv& env, gameserver::model::gameobjects::Npc& npc,
		gameserver::model::gameobjects::Npc& target);

	/**
	 * Schedule new following checker task
	 *
	 * @param player
	 * @param npc
	 * @param npcTargetId
	 * @return
	 * @throws IllegalArgumentException "Supplied npc doesn't exist: " when no spawn of npcTargetId is known on any map
	 */
	static runtime::FutureRef newFollowingToTargetCheckTask(model::QuestEnv& env, gameserver::model::gameobjects::Npc& npc, int32_t npcTargetId);

	/**
	 * Schedule new following checker task
	 *
	 * @param env
	 * @param x
	 * @param y
	 * @param z
	 * @return
	 */
	static runtime::FutureRef newFollowingToTargetCheckTask(model::QuestEnv& env, gameserver::model::gameobjects::Npc& npc, float x, float y,
		float z);

	static runtime::FutureRef newFollowingToTargetCheckTask(model::QuestEnv& env, gameserver::model::gameobjects::Npc& npc,
		const world::zone::ZoneName* zoneName);
};

} // namespace aion::gameserver::questEngine::task
