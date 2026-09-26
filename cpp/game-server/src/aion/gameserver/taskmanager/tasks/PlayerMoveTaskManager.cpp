#include "aion/gameserver/taskmanager/tasks/PlayerMoveTaskManager.h"

#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::taskmanager::tasks {

PlayerMoveTaskManager::PlayerMoveTaskManager() : AbstractPeriodicTaskManager(200, "PlayerMoveTaskManager") {
}

PlayerMoveTaskManager::~PlayerMoveTaskManager() = default;

void PlayerMoveTaskManager::addPlayer(model::gameobjects::Creature& player) {
	movingPlayers.put(player.getObjectId(), runtime::Ref<model::gameobjects::Creature>(player));
}

void PlayerMoveTaskManager::removePlayer(model::gameobjects::Creature& player) {
	movingPlayers.remove(player.getObjectId());
}

void PlayerMoveTaskManager::run() {
	for (runtime::Ptr<model::gameobjects::Creature> player : movingPlayers.values()) {
		if (player->isSpawned())
			player->getMoveController()->moveToDestination();
		else
			removePlayer(*player);
	}
}

PlayerMoveTaskManager& PlayerMoveTaskManager::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<PlayerMoveTaskManager>& instance = *new runtime::Ref<PlayerMoveTaskManager>(runtime::makeRef<PlayerMoveTaskManager>());
	return *instance;
}

} // namespace aion::gameserver::taskmanager::tasks
