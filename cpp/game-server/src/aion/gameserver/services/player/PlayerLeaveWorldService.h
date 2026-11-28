#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author ATracer, Neon
 */
class PlayerLeaveWorldService {
	class LeaveWorldTask;

	// defined in PlayerLeaveWorldService.cpp, where Player and LeaveWorldTask are complete
	static runtime::ConcurrentHashMap<runtime::Ref<model::gameobjects::player::Player>, runtime::Ref<PlayerLeaveWorldService::LeaveWorldTask>>
		leaveWorldTaskByPlayer;

public:
	/**
	 * Registers a player to leave the world 10 seconds after their last action. If their last action was more than 10 seconds ago, the player will
	 * leave the world immediately and the task will be run synchronously. If the player has already been registered, the existing task will not be
	 * rescheduled.
	 */
	static void registerLeaveWorld(model::gameobjects::player::Player& player);

	static bool isLeavingWorld(int32_t playerObjectId);

	static bool isLeavingWorld(model::gameobjects::player::Player& player);

	static void processPendingLeaveWorldTasks();

	/** C++ only (tests): the private leaveWorld, synchronously and with its exceptions */
	static void leaveWorldForTests(model::gameobjects::player::Player& player) { leaveWorld(player); }

private:
	static void leaveWorld(model::gameobjects::player::Player& player);

	// Java implements Runnable
	class LeaveWorldTask : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		const runtime::Ref<model::gameobjects::player::Player> player;
		runtime::Field<runtime::FutureRef> future{};
		runtime::Field<bool> finished{};

	protected:
		explicit LeaveWorldTask(model::gameobjects::player::Player& player);

	public:
		static runtime::Ref<PlayerLeaveWorldService::LeaveWorldTask> create(model::gameobjects::player::Player& player);
		void scheduleOrRun(int64_t delayMillis);
		void run(); // @Override of a Java library type

	protected:
		~LeaveWorldTask() override;
	};
};

} // namespace aion::gameserver::services::player
