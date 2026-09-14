#include "aion/gameserver/world/WorldMapInstance.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor and the destructor need the complete member types. Npc, Player and
// InstanceHandler are hubs of other S0b groups; WorldMap and GeneralTeam are not hubs (their headers come with their chunks). Remove the guard
// once they exist.
#if __has_include("aion/gameserver/instance/handlers/InstanceHandler.h") && __has_include("aion/gameserver/model/gameobjects/Npc.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/Player.h") && __has_include("aion/gameserver/model/team/GeneralTeam.h") && \
	__has_include("aion/gameserver/world/WorldMap.h")
#define AION_S0B_WORLD_MAP_INSTANCE_MEMBERS 1
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/world/WorldMap.h"
#else
#define AION_S0B_WORLD_MAP_INSTANCE_MEMBERS 0
#endif

namespace aion::gameserver::world {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.WorldMapInstance");

int32_t WorldMapInstance::regionSize() {
	static const int32_t value = configs::main::WorldConfig::WORLD_REGION_SIZE.load(); // Java: class initialization, after Config.load
	return value;
}

#if AION_S0B_WORLD_MAP_INSTANCE_MEMBERS
WorldMapInstance::WorldMapInstance(WorldMap& parentValue, int32_t instanceIdValue, int32_t maxPlayersValue,
	const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier)
	: parent(parentValue), instanceId(instanceIdValue), maxPlayers(maxPlayersValue) {
	// Java: zones = ZoneService.getInstance().getZoneInstancesByWorldId(parent.getMapId()); instanceHandler = instanceHandlerSupplier.apply(this);
	// initMapRegions();
	AION_UNPORTED();
}

WorldMapInstance::~WorldMapInstance() = default;
#endif

int32_t WorldMapInstance::getMapId() {
	AION_UNPORTED();
}

const model::templates::world::WorldMapTemplate* WorldMapInstance::getTemplate() {
	AION_UNPORTED();
}

runtime::Ptr<MapRegion> WorldMapInstance::getRegion(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void WorldMapInstance::addObject(model::gameobjects::VisibleObject& object) {
	// task lambda WorldMapInstance@L125:72 (fieldmap.py --class key com.aionemu.gameserver.world.WorldMapInstance@L125:72)
	AION_UNPORTED();
}

void WorldMapInstance::removeObject(model::gameobjects::AionObject& object) {
	AION_UNPORTED();
}

int32_t WorldMapInstance::getPlayerCount() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> WorldMapInstance::getPlayersInside() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> WorldMapInstance::getPlayer(int32_t objId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> WorldMapInstance::getObject(int32_t objId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Npc> WorldMapInstance::getNpc(int32_t npcId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::Npc>> WorldMapInstance::getNpcs(std::initializer_list<int32_t> npcIds) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::Npc>> WorldMapInstance::getNpcs() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> WorldMapInstance::getObjectByStaticId(int32_t staticId) {
	AION_UNPORTED();
}

void WorldMapInstance::detachInstanceHandler() {
	// C++ only: instanceHandler.set(<the no-op InstanceHandler>) (P5-13 with InstanceService.destroyInstance)
	AION_UNPORTED();
}

void WorldMapInstance::releaseRegisteredTeam() noexcept {
	registeredTeam.set(runtime::Ptr<model::team::GeneralTeam>());
}

bool WorldMapInstance::isBeginnerInstance() {
	AION_UNPORTED();
}

void WorldMapInstance::registerTeam(model::team::GeneralTeam& team) {
	AION_UNPORTED();
}

void WorldMapInstance::register_(int32_t objectId) {
	AION_UNPORTED();
}

int32_t WorldMapInstance::getRegisteredCount() {
	AION_UNPORTED();
}

bool WorldMapInstance::isRegistered(int32_t objectId) {
	AION_UNPORTED();
}

void WorldMapInstance::setEmptyInstanceTask(runtime::Ptr<runtime::Future> value) {
	emptyInstanceTask.set(value);
}

void WorldMapInstance::setStartPos(runtime::Ptr<WorldPosition> value) {
	startPos.set(value);
}

std::vector<runtime::Ptr<zone::ZoneInstance>> WorldMapInstance::filterZones(int32_t mapId, int32_t regionId, float startX, float startY, float minZ,
	float maxZ) {
	AION_UNPORTED();
}

bool WorldMapInstance::isInsideZone(model::gameobjects::VisibleObject& object, const zone::ZoneName* zoneName) {
	AION_UNPORTED();
}

bool WorldMapInstance::isInsideZone(WorldPosition& pos, const zone::ZoneName* zoneName) {
	AION_UNPORTED();
}

bool WorldMapInstance::isFull() {
	AION_UNPORTED();
}

void WorldMapInstance::setDoorState(int32_t staticId, bool open) {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::Ptr<model::gameobjects::VisibleObject>> WorldMapInstance::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::VisibleObject>> WorldMapInstance::begin() {
	AION_UNPORTED();
}

void WorldMapInstance::forEachNpc(const std::function<void(model::gameobjects::Npc&)>& consumer) {
	AION_UNPORTED();
}

void WorldMapInstance::forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer) {
	AION_UNPORTED();
}

void WorldMapInstance::forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer) {
	AION_UNPORTED();
}

void WorldMapInstance::forEachDoor(const std::function<void(model::gameobjects::StaticDoor&)>& consumer) {
	AION_UNPORTED();
}

std::string WorldMapInstance::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world
