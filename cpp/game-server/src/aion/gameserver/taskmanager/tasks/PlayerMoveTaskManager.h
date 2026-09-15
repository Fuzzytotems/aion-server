#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/fwd.h"

namespace aion::gameserver::taskmanager::tasks {

/**
 * Moves the moving players every 200 ms.
 * <p>
 * C++: RefCounted through AbstractPeriodicTaskManager; the singleton is a never-released Ref (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class PlayerMoveTaskManager : public AbstractPeriodicTaskManager {
	AION_MAKE_REF_FRIEND
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::Creature>> movingPlayers{
		AION_LOCK_CLASS(PlayerMoveTaskManager::movingPlayers#stripe)};

	PlayerMoveTaskManager();
	~PlayerMoveTaskManager() override;

public:
	void addPlayer(model::gameobjects::Creature& player);

	void removePlayer(model::gameobjects::Creature& player);

	void run() override;

	static PlayerMoveTaskManager& getInstance();
};

} // namespace aion::gameserver::taskmanager::tasks
