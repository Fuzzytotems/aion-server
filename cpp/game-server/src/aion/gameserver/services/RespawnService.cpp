#include "aion/gameserver/services/RespawnService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.RespawnService");

// Java implements Runnable. A task object of schedule with only immutable members (fieldmap K3): ported as a TaskStruct value
// (runtime-architecture.md §7.3, §14.2(f)).
class RespawnService::DecayTask {
public:
	const int32_t objectId;

	explicit DecayTask(int32_t objectId);
	void run();
};

RespawnService::DecayTask::DecayTask(int32_t value) : objectId(value) {
}

void RespawnService::DecayTask::run() {
	AION_UNPORTED();
}

RespawnService::RespawnTask::RespawnTask(model::gameobjects::VisibleObject& object)
	// ID of corpse or already despawned object
	: spawnTemplate(object.getSpawn()), instanceId(object.getInstanceId()), oldObjectId(object.getObjectId()) {
}

runtime::Ref<RespawnService::RespawnTask> RespawnService::RespawnTask::create(model::gameobjects::VisibleObject& object) {
	return runtime::makeRef<RespawnService::RespawnTask>(object);
}

void RespawnService::RespawnTask::run() {
	AION_UNPORTED();
}

bool RespawnService::RespawnTask::tryRegisterOnEventEndTask() {
	AION_UNPORTED();
}

void RespawnService::RespawnTask::respawn() {
	AION_UNPORTED();
}

bool RespawnService::RespawnTask::setReleaseIdOnCompletion() {
	AION_UNPORTED();
}

void RespawnService::RespawnTask::onUnregister() {
	AION_UNPORTED();
}

void RespawnService::RespawnTask::cancel() {
	AION_UNPORTED();
}

void RespawnService::RespawnTask::unregister() {
	AION_UNPORTED();
}

RespawnService::RespawnTask::~RespawnTask() = default;

runtime::FutureRef RespawnService::scheduleDecayTask(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

runtime::FutureRef RespawnService::scheduleDecayTask(model::gameobjects::VisibleObject& visibleObject, int64_t delay) {
	AION_UNPORTED();
}

runtime::Ptr<RespawnService::RespawnTask> RespawnService::scheduleRespawn(model::gameobjects::VisibleObject& visibleObject) {
	AION_UNPORTED();
}

bool RespawnService::hasRespawnTask(model::gameobjects::VisibleObject& visibleObject) {
	AION_UNPORTED();
}

bool RespawnService::setAutoReleaseId(int32_t objectId) {
	AION_UNPORTED();
}

void RespawnService::cancelRespawn(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

bool RespawnService::cancelRespawn(int32_t objectId, model::templates::spawns::SpawnTemplate& spawnTemplate) {
	AION_UNPORTED();
}

int32_t RespawnService::cancelRespawns(const std::function<bool(model::templates::spawns::SpawnTemplate&)>& predicate) {
	AION_UNPORTED();
}

int32_t RespawnService::cancelEventRespawns(const model::templates::event::EventTemplate* eventTemplate) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
