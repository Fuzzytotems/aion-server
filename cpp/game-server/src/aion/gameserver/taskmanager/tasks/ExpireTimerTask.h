#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/fwd.h"

namespace aion::gameserver::taskmanager::tasks {

/**
 * Checks the registered expirables (items, house objects, emotions, motions, titles, pets) every second, warns their owners before they expire
 * and expires them.
 * <p>
 * C++: RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released Ref (hub-headers.md §11.1), created on the first
 * getInstance() like Java's SingletonHolder (its constructor schedules run() on the ThreadPoolManager, so the runtime must be started). The map
 * retains the expirables and their players until unregisterExpirables(player) (PlayerLeaveWorldService) or their expiry, as in Java.
 * registerExpirables takes the §7.1 vector; the C++-only template overload accepts any range of Ptr/Ref/reference-like elements, so callers can
 * pass `inventory.getItems()` without building a vector of the interface type.
 *
 * @author Mr. Poke
 */
class ExpireTimerTask : public AbstractPeriodicTaskManager {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<runtime::Ref<model::Expirable>, runtime::Ref<model::gameobjects::player::Player>> expirables{
		AION_LOCK_CLASS(ExpireTimerTask::expirables#stripe)};

protected:
	ExpireTimerTask();
	~ExpireTimerTask() override;

public:
	static ExpireTimerTask& getInstance();

	void registerExpirable(model::Expirable& expirable, model::gameobjects::player::Player& player);

	void registerExpirables(const std::vector<runtime::Ptr<model::Expirable>>& expirables, model::gameobjects::player::Player& player);

	/** C++ only: registerExpirables for any range whose elements dereference to an Expirable (Java Collection<? extends Expirable>) */
	template <class Range>
	void registerExpirables(const Range& range, model::gameobjects::player::Player& player) {
		for (const auto& expirable : range)
			registerExpirable(*expirable, player);
	}

	void unregisterExpirables(model::gameobjects::player::Player& player);

	void run() override;
};

} // namespace aion::gameserver::taskmanager::tasks
