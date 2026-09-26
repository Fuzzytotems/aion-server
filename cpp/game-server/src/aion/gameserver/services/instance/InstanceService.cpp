#include "aion/gameserver/services/instance/InstanceService.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/instance/InstanceScaler.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::instance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.instance.InstanceService");

// Defined here (hub-headers.md §9.3): only InstanceService bodies use it. Scheduled at a fixed rate and kept in
// WorldMapInstance.emptyInstanceTask, it reads its instance on the pool thread in every run, so it is K4 (fieldmap.toml [kinds]):
// RefCounted, created with create(), retaining the instance. The cycle WorldMapInstance.emptyInstanceTask -> Future -> task -> instance is
// cut when destroyInstance cancels the task (InstanceService.java destroyInstance).
// Java implements Runnable
class InstanceService::EmptyInstanceCheckerTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<world::WorldMapInstance> worldMapInstance;
	const int64_t taskStartTime;

protected:
	explicit EmptyInstanceCheckerTask(world::WorldMapInstance& worldMapInstance);
	~EmptyInstanceCheckerTask() override;

public:
	/** Java: new EmptyInstanceCheckerTask(worldMapInstance) */
	static runtime::Ref<EmptyInstanceCheckerTask> create(world::WorldMapInstance& worldMapInstance);
	bool canDestroyInstance();
	bool isRegisteredTeamDisbanded();
	int64_t calculateDestroyTime();
	void run(); // @Override of a Java library type
};

InstanceService::EmptyInstanceCheckerTask::EmptyInstanceCheckerTask(world::WorldMapInstance& value)
	: worldMapInstance(value), taskStartTime(commons::utils::currentTimeMillis()) {
}

InstanceService::EmptyInstanceCheckerTask::~EmptyInstanceCheckerTask() = default;

runtime::Ref<InstanceService::EmptyInstanceCheckerTask> InstanceService::EmptyInstanceCheckerTask::create(world::WorldMapInstance& value) {
	return runtime::makeRef<EmptyInstanceCheckerTask>(value);
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
	// TODO(P5-13): when this body is ported (wave 5b), erase the instance from InstanceScaler::scalings. Java's map is a WeakHashMap, so the
	// entry disappears with the instance; the port uses strong keys (InstanceScaler.h, docs/deviations/P5-13.md "InstanceScaler.scalings"), so
	// without the erase every scaled WorldMapInstance, its Scaling and its stat functions stay alive for the life of the process.
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrRegisterInstance(int32_t worldId, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getRegisteredInstance(int32_t worldId, int32_t objectId) {
	for (const runtime::Ptr<world::WorldMapInstance>& instance : *world::World::getInstance().getWorldMap(worldId)) {
		if (instance->isRegistered(objectId))
			return instance;
	}
	return nullptr;
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrCreateHouseInstance(model::house::House& house) {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrCreatePersonalInstance(int32_t worldId, int32_t ownerId) {
	// Java: WorldMapType.getWorld(worldId).isPersonal() (NullPointerException for a map id without a WorldMapType constant)
	std::optional<world::WorldMapType> worldMapType = world::getWorldMapType(worldId);
	if (ownerId != 0 && !worldMapType)
		throw runtime::NullPointerException("WorldMapType.getWorld(" + std::to_string(worldId) + ")");
	if (ownerId == 0 || !world::isPersonal(*worldMapType))
		return nullptr;

	for (const runtime::Ptr<world::WorldMapInstance>& instance : *world::World::getInstance().getWorldMap(worldId)) {
		if (instance->isPersonal() && instance->getOwnerId() == ownerId)
			return instance;
	}
	// Java: return getNextAvailableInstance(worldId, ownerId, (byte) 0, 0, true);
	AION_PARTIAL("personal instances are not created: InstanceService.getNextAvailableInstance is not ported (M5a)");
	return nullptr;
}

void InstanceService::onPlayerLogin(model::gameobjects::player::Player& player) {
	int32_t worldId = player.getWorldId();
	int32_t ownerId = player.getCommonData()->getWorldOwnerId();
	runtime::Ptr<world::WorldMapInstance> instance =
		ownerId != 0 ? getOrCreatePersonalInstance(worldId, ownerId) : getRegisteredInstance(worldId, player.getObjectId());
	if ((!instance && player.getWorldMapInstance()->getTemplate()->isInstance()) || (instance && instance->isFull()))
		moveToExitPoint(player);
	else if (instance) // set to correct instanceId (default on login is 1)
		world::World::getInstance().setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(player), worldId, instance->getInstanceId(), player.getX(),
			player.getY(), player.getZ(), player.getHeading());
	player.getWorldMapInstance()->getInstanceHandler()->onPlayerLogin(player);
}

void InstanceService::moveToExitPoint(model::gameobjects::player::Player& player) {
	// Java: TeleportService.moveToInstanceExit(player, player.getWorldId(), player.getRace());
	static_cast<void>(player);
	AION_PARTIAL("players are not moved to the instance exit on login: TeleportService.moveToInstanceExit is not ported (M5a)");
}

bool InstanceService::instanceExists(int32_t worldId, int32_t instanceId) {
	return world::World::getInstance().getWorldMap(worldId)->getWorldMapInstance(instanceId) != nullptr;
}

void InstanceService::onLogout(model::gameobjects::player::Player& player) {
	player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onPlayerLogout(player);
}

void InstanceService::onEnterInstance(model::gameobjects::player::Player& player) {
	player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onEnterInstance(player);
	AutoGroupService::getInstance().onEnterInstance(player);
	InstanceScaler::onEnterInstance(player);
}

void InstanceService::onLeaveInstance(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void InstanceService::onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onEnterZone(player, zone);
}

void InstanceService::onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onLeaveZone(player, zone);
}

int32_t InstanceService::getInstanceRate(model::gameobjects::player::Player& player, int32_t mapId) {
	using configs::main::InstanceConfig;
	std::shared_ptr<const std::unordered_set<int32_t>> excludedMaps = InstanceConfig::INSTANCE_COOLDOWN_RATE_EXCLUDED_MAPS.get();
	return player.hasPermission(configs::main::MembershipConfig::INSTANCES_COOLDOWN.load()) && !(excludedMaps && excludedMaps->contains(mapId))
		? InstanceConfig::INSTANCE_COOLDOWN_RATE.load()
		: 1;
}

int32_t InstanceService::getDestroyDelaySeconds(world::WorldMapInstance& worldMapInstance) {
	return worldMapInstance.getMaxPlayers() == 1 ? configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.load()
												 : configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.load();
}

} // namespace aion::gameserver::services::instance
