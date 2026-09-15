#include "aion/gameserver/taskmanager/tasks/MoveTaskManager.h"

#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

namespace aion::gameserver::taskmanager::tasks {

MoveTaskManager::MoveTaskManager() : AbstractPeriodicTaskManager(UPDATE_PERIOD, "MoveTaskManager") {
}

MoveTaskManager::~MoveTaskManager() = default;

void MoveTaskManager::addCreature(model::gameobjects::Creature& creature) {
	if (!creature.isSpawned()) { // log with stack trace to find the cause
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.taskmanager.tasks.MoveTaskManager")
			.warn("Failed attempt to add " + creature.toString() + " to moving creatures (despawned objects cannot move)",
				runtime::UnsupportedOperationException(""));
		return;
	}
	movingCreatures.putIfAbsent(creature.getObjectId(), runtime::Ref<model::gameobjects::Creature>(creature));
}

bool MoveTaskManager::removeCreature(model::gameobjects::Creature& creature) {
	return static_cast<bool>(movingCreatures.remove(creature.getObjectId()));
}

void MoveTaskManager::run() {
	std::vector<runtime::Ptr<model::gameobjects::Creature>> creatures = movingCreatures.values().toVector();
	// Java: movingCreatures.values().parallelStream().forEach(...)
	runtime::ForkJoinPool::commonPool().parallelForEach(creatures, [this](const runtime::Ptr<model::gameobjects::Creature>& creature) {
		if (!creature->isSpawned()) { // can despawn concurrently, while this thread is already running
			if (removeCreature(*creature)) // should have been removed via onDespawn (MoveController#abortMove())
				commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.taskmanager.tasks.MoveTaskManager")
					.warn(creature->toString() + " was still in moving creatures list but already despawned");
			return;
		}
		creature->getMoveController()->moveToDestination();
		if (creature->getAi().isDestinationReached()) {
			removeCreature(*creature);
			creature->getAi().onGeneralEvent(ai::event::AIEventType::MOVE_ARRIVED);
			world::zone::ZoneUpdateService::getInstance().add(*creature);
		} else {
			creature->getAi().onGeneralEvent(ai::event::AIEventType::MOVE_VALIDATE);
		}
	});
}

MoveTaskManager& MoveTaskManager::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<MoveTaskManager>& instance = *new runtime::Ref<MoveTaskManager>(runtime::makeRef<MoveTaskManager>());
	return *instance;
}

} // namespace aion::gameserver::taskmanager::tasks
