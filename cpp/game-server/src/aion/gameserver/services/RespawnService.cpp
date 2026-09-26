#include "aion/gameserver/services/RespawnService.h"

#include <algorithm>
#include <memory>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnMap.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.RespawnService");

// Java implements Runnable. A task object of schedule with only immutable members (fieldmap K3): ported as an aggregate TaskStruct value
// (runtime-architecture.md §7.3, §14.2(f)), so the unpinned schedule form accepts it.
class RespawnService::DecayTask : public runtime::TaskStruct {
public:
	const int32_t objectId;

	void operator()() const { run(); }
	void run() const;
};

void RespawnService::DecayTask::run() const {
	runtime::Ptr<model::gameobjects::VisibleObject> visibleObject = world::World::getInstance().findVisibleObject(objectId);
	if (visibleObject) {
		visibleObject->getController().delete_();
	}
}

RespawnService::RespawnTask::RespawnTask(model::gameobjects::VisibleObject& object)
	// ID of corpse or already despawned object
	: spawnTemplate(object.getSpawn()), instanceId(object.getInstanceId()), oldObjectId(object.getObjectId()) {
}

runtime::Ref<RespawnService::RespawnTask> RespawnService::RespawnTask::create(model::gameobjects::VisibleObject& object) {
	return runtime::makeRef<RespawnService::RespawnTask>(object);
}

void RespawnService::RespawnTask::run() {
	if (tryRegisterOnEventEndTask()) {
		future.set(runtime::FutureRef());
		return;
	}
	unregister();
	respawn();
}

bool RespawnService::RespawnTask::tryRegisterOnEventEndTask() {
	if (spawnTemplate->isEventSpawn())
		return false;
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<event::Event>>> activeEvents = event::EventService::getInstance().getActiveEvents();
	for (runtime::Ptr<event::Event> activeEvent : *activeEvents) {
		if (activeEvent->getEventTemplate()->getSpawns() == nullptr)
			continue;
		// if a currently active event contains an event spawn with custom="true" for this non-event spawn, we register it for respawn when the event ends
		const auto& spawnMaps = activeEvent->getEventTemplate()->getSpawns()->getTemplates();
		bool custom = std::any_of(spawnMaps.begin(), spawnMaps.end(), [this](const model::templates::spawns::SpawnMap& m) {
			return m.getMapId() == spawnTemplate->getWorldId() &&
				std::any_of(m.getSpawns().begin(), m.getSpawns().end(), [this](const std::unique_ptr<model::templates::spawns::Spawn>& spawn) {
					return spawn->getNpcId() == spawnTemplate->getNpcId() && spawn->isCustom();
				});
		});
		if (custom) {
			runtime::Ref<RespawnTask> self(*this);
			return activeEvent->addOnEventEndTask(runtime::PinnedCallback<void()>(runtime::Pin(), [self] { self->run(); }));
		}
	}
	return false;
}

void RespawnService::RespawnTask::respawn() {
	if (!instance::InstanceService::instanceExists(spawnTemplate->getWorldId(), instanceId))
		return;

	runtime::Ptr<model::templates::spawns::SpawnTemplate> template_ = spawnTemplate->hasPool() ? spawnTemplate->changeTemplate(instanceId)
																								: runtime::Ptr<model::templates::spawns::SpawnTemplate>(spawnTemplate);
	runtime::Ptr<model::gameobjects::VisibleObject> respawn = spawnengine::SpawnEngine::spawnObject(*template_, instanceId);
	if (respawn) {
		RiftService::getInstance().updateSpawned(oldObjectId, *respawn);
		if (respawn->getSpawn()->isTemporarySpawn() && respawn->getObjectId() != oldObjectId)
			spawnengine::TemporarySpawnEngine::unregisterSpawned(oldObjectId);
	}
}

bool RespawnService::RespawnTask::setReleaseIdOnCompletion() {
	SYNCHRONIZED(*this) {
		runtime::Ptr<RespawnTask> pending = pendingRespawns.get(oldObjectId);
		if (pending.get() == this) { // unregistering not yet happened (Java: this.equals(...), identity)
			releaseIdOnUnregister.set(true);
			return true;
		}
	}
	return false;
}

void RespawnService::RespawnTask::onUnregister() {
	if (releaseIdOnUnregister.get())
		utils::idfactory::IDFactory::getInstance().releaseId(oldObjectId);
}

void RespawnService::RespawnTask::cancel() {
	unregister();
	future.get()->cancel(false);
}

void RespawnService::RespawnTask::unregister() {
	SYNCHRONIZED(*this) {
		if (pendingRespawns.remove(oldObjectId, runtime::Ref<RespawnTask>(*this)))
			onUnregister();
	}
}

RespawnService::RespawnTask::~RespawnTask() = default;

runtime::FutureRef RespawnService::scheduleDecayTask(model::gameobjects::Npc& npc) {
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> drop =
		drop::DropRegistrationService::getInstance().getCurrentDropMap().get(npc.getObjectId());
	int32_t decayInterval;
	if (!drop || drop->isEmpty())
		decayInterval = IMMEDIATE_DECAY;
	else
		decayInterval = WITH_DROP_DECAY;
	return scheduleDecayTask(npc, decayInterval);
}

runtime::FutureRef RespawnService::scheduleDecayTask(model::gameobjects::VisibleObject& visibleObject, int64_t delay) {
	if (delay == 0)
		delay = IMMEDIATE_DECAY; // always delay, to show death animation
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule(DecayTask{{}, visibleObject.getObjectId()}, delay);
	if (auto* creature = dynamic_cast<model::gameobjects::Creature*>(&visibleObject))
		creature->getController().addTask(model::TaskId::DECAY, task);
	return task;
}

runtime::Ptr<RespawnService::RespawnTask> RespawnService::scheduleRespawn(model::gameobjects::VisibleObject& visibleObject) {
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawnTemplate = visibleObject.getSpawn();
	if (!spawnTemplate || spawnTemplate->isNoRespawn())
		return nullptr;
	runtime::Ref<RespawnTask> respawnTask = RespawnTask::create(visibleObject);
	respawnTask->future.set(utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(), [respawnTask] { respawnTask->run(); },
		static_cast<int64_t>(spawnTemplate->getRespawnTime()) * 1000));
	runtime::Ptr<RespawnTask> oldRespawnTask = pendingRespawns.put(visibleObject.getObjectId(), respawnTask);
	if (oldRespawnTask) { // objectId should not have been in pendingRespawns
		if (spawnTemplate == oldRespawnTask->spawnTemplate) {
			log.warn("Duplicate respawn task initiated for {}", visibleObject.toString(), runtime::IllegalStateException(""));
		} else {
			log.warn("ObjectId {} got released and reassigned while there was a still active respawn task for the old objectId owner.\nOld owner: Npc "
					 "ID: {}, map ID: {}\nNew owner: {}",
				visibleObject.getObjectId(), oldRespawnTask->spawnTemplate->getNpcId(), oldRespawnTask->spawnTemplate->getWorldId(),
				visibleObject.toString());
		}
	}
	return respawnTask;
}

bool RespawnService::hasRespawnTask(model::gameobjects::VisibleObject& visibleObject) {
	return pendingRespawns.containsKey(visibleObject.getObjectId());
}

bool RespawnService::setAutoReleaseId(int32_t objectId) {
	runtime::Ptr<RespawnTask> respawn = pendingRespawns.get(objectId);
	if (respawn)
		return respawn->setReleaseIdOnCompletion();
	return false;
}

void RespawnService::cancelRespawn(model::gameobjects::VisibleObject& object) {
	runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = object.getSpawn();
	// Java: cancelRespawn(object.getObjectId(), object.getSpawn()) - a null spawn template matches no respawn task
	if (!spawn)
		return;
	cancelRespawn(object.getObjectId(), *spawn);
}

bool RespawnService::cancelRespawn(int32_t objectId, model::templates::spawns::SpawnTemplate& spawnTemplate) {
	runtime::Ptr<RespawnTask> respawnTask = pendingRespawns.get(objectId);
	if (respawnTask && respawnTask->future.get() && respawnTask->spawnTemplate.get() == &spawnTemplate) {
		respawnTask->cancel();
		return true;
	}
	return false;
}

int32_t RespawnService::cancelRespawns(const std::function<bool(model::templates::spawns::SpawnTemplate&)>& predicate) {
	int32_t count = 0;
	for (runtime::Ptr<RespawnTask> respawn : pendingRespawns.values()) {
		if (predicate(*respawn->spawnTemplate)) {
			respawn->cancel();
			count++;
		}
	}
	return count;
}

int32_t RespawnService::cancelEventRespawns(const model::templates::event::EventTemplate* eventTemplate) {
	// Java: eventTemplate.equals(spawnTemplate.getEventTemplate()) - identity (EventTemplate does not override equals)
	return cancelRespawns([eventTemplate](model::templates::spawns::SpawnTemplate& spawnTemplate) {
		return eventTemplate == spawnTemplate.getEventTemplate();
	});
}

} // namespace aion::gameserver::services
