#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/fwd.h"

namespace aion::gameserver::taskmanager::tasks {

/**
 * Moves the creatures that are walking towards a destination, every 200 ms on the ForkJoin pool.
 * <p>
 * C++: RefCounted through AbstractPeriodicTaskManager (fieldmap K4); the singleton is a never-released Ref (hub-headers.md §11.1), so the
 * periodic task that pins it never keeps a dying object.
 *
 * @author ATracer, Rolandas
 */
class MoveTaskManager : public AbstractPeriodicTaskManager {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t UPDATE_PERIOD = 200;
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::Creature>> movingCreatures{AION_LOCK_CLASS(MoveTaskManager::movingCreatures#stripe)};

	MoveTaskManager();
	~MoveTaskManager() override;

public:
	void addCreature(model::gameobjects::Creature& creature);

	bool removeCreature(model::gameobjects::Creature& creature);

	void run() override;

	static MoveTaskManager& getInstance();
};

} // namespace aion::gameserver::taskmanager::tasks
