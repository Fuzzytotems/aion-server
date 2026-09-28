#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/fwd.h"

namespace aion::gameserver::taskmanager::tasks {

/**
 * Keeps the players that may still trade a temporarily tradeable item (a team loot whose template has temp_exchange_time, DropService's
 * TempTradeDropPredicate) and ends the exchange time of every item whose time ran out.
 * <p>
 * C++ (m5c-plan.md P-04, drafted with `skeleton.py --draft`): RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released
 * Ref (hub-headers.md §11.1), created on the first getInstance() like Java's SingletonHolder (its constructor schedules run() on the
 * ThreadPoolManager, so the runtime must be started), as ExpireTimerTask does. The map retains each item and the players' set until the item's
 * exchange time is over, as in Java. The set is the caller's own collection, not a copy: Java stores the reference it is given, and its only
 * caller hands over DropNpc.allowedLooters, which DropNpc.startFreeForAll clears in place (so canTrade answers false for everyone afterwards).
 * The value is therefore `Ref<RcHashSet<int32_t>>` (fieldmap.toml `TemporaryTradeTimeTask.items`), and addTask takes the set by reference.
 * run() reads Java's System.currentTimeMillis() as `utils::ThreadPoolManager::clock()` (SystemClock in production, the ManualClock of a
 * DeterministicExecutor in the tests; docs/deviations/P5-07.md).
 *
 * @author Mr. Poke
 */
class TemporaryTradeTimeTask : public AbstractPeriodicTaskManager {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<runtime::Ref<model::gameobjects::Item>, runtime::Ref<runtime::RcHashSet<int32_t>>> items{
		AION_LOCK_CLASS(TemporaryTradeTimeTask::items#stripe)};

protected:
	TemporaryTradeTimeTask();
	~TemporaryTradeTimeTask() override;

public:
	static TemporaryTradeTimeTask& getInstance(); // Java singleton

	/** @param players the object ids of the players that may trade the item; kept, not copied (class comment) */
	void addTask(model::gameobjects::Item& item, runtime::RcHashSet<int32_t>& players);

	bool canTrade(model::gameobjects::Item& item, int32_t playerObjectId);

	void run() override;
};

} // namespace aion::gameserver::taskmanager::tasks
