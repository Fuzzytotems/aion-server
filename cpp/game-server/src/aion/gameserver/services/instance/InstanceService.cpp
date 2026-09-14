#include "aion/gameserver/services/instance/InstanceService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::services::instance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.instance.InstanceService");

// Defined here (hub-headers.md §9.3): only InstanceService bodies use it. The members are what fieldmap.py prints (K5); the
// port schedules the task, so it becomes a pinned TaskStruct with a Ref member (fieldmap change request).
// Java implements Runnable
class InstanceService::EmptyInstanceCheckerTask {
public:
	runtime::Ptr<world::WorldMapInstance> worldMapInstance{};
	int64_t taskStartTime{};
	explicit EmptyInstanceCheckerTask(world::WorldMapInstance& worldMapInstance);
	bool canDestroyInstance();
	bool isRegisteredTeamDisbanded();
	int64_t calculateDestroyTime();
	void run(); // @Override of a Java library type
};

InstanceService::EmptyInstanceCheckerTask::EmptyInstanceCheckerTask(world::WorldMapInstance& value) {
	AION_UNPORTED();
}

bool InstanceService::EmptyInstanceCheckerTask::canDestroyInstance() {
	AION_UNPORTED();
}

bool InstanceService::EmptyInstanceCheckerTask::isRegisteredTeamDisbanded() {
	AION_UNPORTED();
}

int64_t InstanceService::EmptyInstanceCheckerTask::calculateDestroyTime() {
	AION_UNPORTED();
}

void InstanceService::EmptyInstanceCheckerTask::run() {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, int32_t ownerId, int8_t difficultyId, const std::function<runtime::Ref<gameserver::instance::handlers::InstanceHandler>(world::WorldMapInstance&)>& instanceHandlerSupplier, int32_t maxPlayers, bool autoDestroy) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, int32_t ownerId, int8_t difficult, int32_t maxPlayers, bool autoDestroy) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, int8_t difficult, int32_t maxPlayers) {
	AION_UNPORTED();
}

void InstanceService::destroyInstance(world::WorldMapInstance& instance) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrRegisterInstance(int32_t worldId, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getRegisteredInstance(int32_t worldId, int32_t objectId) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrCreateHouseInstance(model::house::House& house) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrCreatePersonalInstance(int32_t worldId, int32_t ownerId) {
	AION_UNPORTED();
}

void InstanceService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void InstanceService::moveToExitPoint(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool InstanceService::instanceExists(int32_t worldId, int32_t instanceId) {
	AION_UNPORTED();
}

void InstanceService::onLogout(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void InstanceService::onEnterInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void InstanceService::onLeaveInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void InstanceService::onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void InstanceService::onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

int32_t InstanceService::getInstanceRate(model::gameobjects::player::Player& player, int32_t mapId) {
	AION_UNPORTED();
}

int32_t InstanceService::getDestroyDelaySeconds(world::WorldMapInstance& worldMapInstance) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::instance
