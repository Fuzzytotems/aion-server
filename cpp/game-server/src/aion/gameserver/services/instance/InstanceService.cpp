#include "aion/gameserver/services/instance/InstanceService.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/instance/InstanceEngine.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/BuildingType.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/InstanceScaler.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"
#include "aion/gameserver/spawnengine/WalkerFormator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/WorldType.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::instance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.instance.InstanceService");

namespace {

/**
 * The erase of D13 (docs/deviations/P5-13.md "InstanceScaler.scalings"): Java's WeakHashMap drops a destroyed instance by itself, the port's
 * strong-keyed map must be told. `InstanceScaler::scalings` is private and InstanceScaler.h declares no erase (header request m5f-asc-1,
 * docs/porting/header-requests.md), so this file reaches the map through the standard's explicit-instantiation rule: the names in an explicit
 * instantiation are not access-checked ([temp.spec.general]/6), so instantiating ScalingsAccess with `&InstanceScaler::scalings` defines the
 * friend `scalingsOf(ScalingsTag)` that hands the map's address out (the pattern of tests/effects_al/EffectTemplateTest.cpp:114-129). A renamed
 * or retyped member stops compiling here instead of being skipped. Replaced by the requested `InstanceScaler::onInstanceDestroy` once applied.
 */
struct ScalingsTag {
	using Type = runtime::HashMap<runtime::Ref<world::WorldMapInstance>, runtime::Ref<InstanceScaler::Scaling>>*;
	friend Type scalingsOf(ScalingsTag);
};

template <class Tag, typename Tag::Type Member>
struct ScalingsAccess {
	friend typename Tag::Type scalingsOf(Tag) { return Member; }
};

template struct ScalingsAccess<ScalingsTag, &InstanceScaler::scalings>;

} // namespace

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

// Java InstanceService.java:178-182
bool InstanceService::EmptyInstanceCheckerTask::canDestroyInstance() {
	if (!worldMapInstance->getPlayersInside().empty())
		return false;
	return worldMapInstance->isPersonal() || isRegisteredTeamDisbanded() || commons::utils::currentTimeMillis() > calculateDestroyTime() - 1000;
}

// Java InstanceService.java:184-187
bool InstanceService::EmptyInstanceCheckerTask::isRegisteredTeamDisbanded() {
	runtime::Ptr<model::team::GeneralTeam> registeredTeam = worldMapInstance->getRegisteredTeam();
	return registeredTeam && registeredTeam->isDisbanded();
}

// Java InstanceService.java:189-192
int64_t InstanceService::EmptyInstanceCheckerTask::calculateDestroyTime() {
	int64_t lastActivity = std::max(taskStartTime, worldMapInstance->getLastPlayerLeaveTime());
	// Java `getDestroyDelaySeconds(worldMapInstance) * 1000` is an int product (it wraps past 24 days): spelled with the wrap, not C++'s UB
	return lastActivity + static_cast<int32_t>(static_cast<uint32_t>(getDestroyDelaySeconds(*worldMapInstance)) * 1000u);
}

// Java InstanceService.java:194-198
void InstanceService::EmptyInstanceCheckerTask::run() {
	if (canDestroyInstance())
		destroyInstance(*worldMapInstance);
}

// Java InstanceService.java:39-62
runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, int32_t ownerId, int8_t difficultyId, const std::function<runtime::Ref<gameserver::instance::handlers::InstanceHandler>(world::WorldMapInstance&)>& instanceHandlerSupplier, int32_t maxPlayers, bool autoDestroy) {
	runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(worldId); // Java: a map id World does not know is an NPE below

	if (!map->isInstanceType() || (map->getWorldType() == world::WorldType::PANESTERRA && !map->getAvailableInstanceIds().empty()))
		throw runtime::UnsupportedOperationException("Invalid call for next available instance  of " + std::to_string(worldId));

	runtime::Ptr<world::WorldMapInstance> instance;
	if (!instanceHandlerSupplier) { // Java: instanceHandlerSupplier == null
		instance = world::WorldMapInstanceFactory::createWorldMapInstance(*map, ownerId,
			[](world::WorldMapInstance& mapInstance) { return gameserver::instance::InstanceEngine::getInstance().getNewInstanceHandler(mapInstance); },
			maxPlayers);
		spawnengine::SpawnEngine::spawnInstance(*instance, difficultyId, ownerId);
	} else {
		instance = world::WorldMapInstanceFactory::createWorldMapInstance(*map, ownerId, instanceHandlerSupplier, maxPlayers);
		// Java: getActiveEvents().stream().map(Event::getEventTemplate).filter(t -> t.getSpawns() != null).forEach(t -> spawnEventSpawns(...))
		runtime::Ptr<runtime::RcHashSet<runtime::Ref<event::Event>>> activeEvents = event::EventService::getInstance().getActiveEvents();
		for (runtime::Ptr<event::Event> activeEvent : *activeEvents) {
			const model::templates::event::EventTemplate* eventTemplate = activeEvent->getEventTemplate();
			if (eventTemplate->getSpawns() != nullptr)
				spawnengine::SpawnEngine::spawnEventSpawns(*instance, difficultyId, ownerId, eventTemplate);
		}
	}
	instance->getInstanceHandler()->onInstanceCreate();

	// finally start the checker
	if (autoDestroy) {
		runtime::Ref<EmptyInstanceCheckerTask> checker = EmptyInstanceCheckerTask::create(*instance);
		// Java: scheduleAtFixedRate(new EmptyInstanceCheckerTask(instance), 60000, 60000). The Pin keeps the task (and through it the instance) as
		// Java's queued Runnable does, until destroyInstance cancels the future (cycles.toml EmptyInstanceCheckerTask.worldMapInstance).
		instance->setEmptyInstanceTask(
			utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin(checker), [task = checker.get()] { task->run(); }, 60000, 60000));
	}

	log.info("Created new instance: " + std::to_string(worldId) + " [" + std::to_string(instance->getInstanceId()) + "] owner:" +
		std::to_string(ownerId) + " difficultyId:" + std::to_string(difficultyId));
	return instance;
}

// Java InstanceService.java:64-66
runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, int32_t ownerId, int8_t difficult, int32_t maxPlayers, bool autoDestroy) {
	return getNextAvailableInstance(worldId, ownerId, difficult, nullptr, maxPlayers, autoDestroy);
}

// Java InstanceService.java:68-73
runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, model::gameobjects::player::Player& player) {
	int32_t maxPlayers = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getMaxMemberCount(worldId, player.getRace());
	runtime::Ptr<world::WorldMapInstance> instance = getNextAvailableInstance(worldId, 0, int8_t{0}, nullptr, maxPlayers, true);
	instance->register_(player.getObjectId());
	return instance;
}

// Java InstanceService.java:75-77
runtime::Ptr<world::WorldMapInstance> InstanceService::getNextAvailableInstance(int32_t worldId, int8_t difficult, int32_t maxPlayers) {
	return getNextAvailableInstance(worldId, 0, difficult, nullptr, maxPlayers, true);
}

// Java InstanceService.java:82-107, then the C++ cycle breakers (cycles.toml "Instance destroy") and D13's scalings erase
void InstanceService::destroyInstance(world::WorldMapInstance& instance) {
	if (runtime::Ptr<runtime::Future> emptyInstanceTask = instance.getEmptyInstanceTask())
		emptyInstanceTask->cancel(false);

	int32_t worldId = instance.getMapId();
	runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(worldId);
	if (!map->isInstanceType())
		return;
	int32_t instanceId = instance.getInstanceId();

	map->removeWorldMapInstance(instanceId);

	log.info("Destroying " + instance.toString());

	spawnengine::TemporarySpawnEngine::onInstanceDestroy(instance); // first unregister all temporary spawns, then despawn mobs
	for (runtime::Ptr<model::gameobjects::VisibleObject> obj : instance) {
		if (runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(obj)) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_FORCE(0));
			moveToExitPoint(*player);
		} else {
			obj->getController().delete_();
		}
	}
	instance.getInstanceHandler()->onInstanceDestroy();
	spawnengine::WalkerFormator::onInstanceDestroy(worldId, instanceId);

	// C++ only (cycles.toml "Instance destroy"): Java lets the collector take the instance, its handler, its start position and its team
	// together; the port cuts the three edges that would keep them alive through each other, then forgets the instance's scaling (D13).
	instance.detachInstanceHandler();
	instance.setStartPos(nullptr);
	instance.releaseRegisteredTeam();
	static_cast<void>(scalingsOf(ScalingsTag{})->remove(runtime::Ref<world::WorldMapInstance>(instance)));
}

// Java InstanceService.java:109-114
runtime::Ptr<world::WorldMapInstance> InstanceService::getOrRegisterInstance(int32_t worldId, model::gameobjects::player::Player& player) {
	runtime::Ptr<world::WorldMapInstance> instance = getRegisteredInstance(worldId, player.getObjectId());
	if (!instance)
		instance = getNextAvailableInstance(worldId, player);
	return instance;
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getRegisteredInstance(int32_t worldId, int32_t objectId) {
	for (const runtime::Ptr<world::WorldMapInstance>& instance : *world::World::getInstance().getWorldMap(worldId)) {
		if (instance->isRegistered(objectId))
			return instance;
	}
	return nullptr;
}

runtime::Ptr<world::WorldMapInstance> InstanceService::getOrCreateHouseInstance(model::house::House& house) {
	// M5h HS-2 under a P5-11 lease (m5h-plan.md 14.2)
	runtime::Ptr<world::WorldMapInstance> instance = !house.getPosition() ? nullptr : house.getPosition()->getWorldMapInstance();
	if (!instance && house.getBuilding()->getType() == model::templates::housing::BuildingType::PERSONAL_INS) { // studio
		instance = getOrCreatePersonalInstance(house.getAddress()->getMapId(), house.getOwnerId());
	}
	if (!instance) // should never happen since only studios are spawned on demand
		throw runtime::NullPointerException(house.toString() + " has no instance");
	return instance;
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
	return getNextAvailableInstance(worldId, ownerId, int8_t{0}, 0, true);
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

// Java InstanceService.java:155-157
void InstanceService::moveToExitPoint(model::gameobjects::player::Player& player) {
	teleport::TeleportService::moveToInstanceExit(player, player.getWorldId(), player.getRace());
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

// Java InstanceService.java:210-224
void InstanceService::onLeaveInstance(model::gameobjects::player::Player& player) {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	runtime::Ptr<world::WorldMapInstance> instance = player.getWorldMapInstance();
	instance->getInstanceHandler()->onLeaveInstance(player);
	if (instance->getRegisteredCount() > 0) {
		if (instance->getMaxPlayers() == 1) // solo instance
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(getDestroyDelaySeconds(*instance) / 60));
		else if (instance->getRegisteredTeam() && instance->getRegisteredTeam()->getMembers().empty())
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_PARTY(0));
		else if (instance->getPlayersInside().size() <= 1)
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_PARTY(getDestroyDelaySeconds(*instance) / 60));
	}

	if (configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.load())
		AutoGroupService::getInstance().onLeaveInstance(player);
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
