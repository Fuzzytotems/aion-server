#include "aion/gameserver/world/World.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/container/PlayerContainer.h"

namespace aion::gameserver::world {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.World");

World::World() : allPlayers(container::PlayerContainer::create()) {
	// Java: DataManager.WORLD_MAPS_DATA.forEachParalllel(template -> { worldMap = new WorldMap(template); synchronized (worldMaps) { put } });
	// ShieldService.getInstance().logDetachedShields(); log.info("World: " + worldMaps.size() + " world maps created.");
	AION_UNPORTED();
}

World::~World() = default;

World& World::getInstance() {
	static World instance; // Java: SingletonHolder
	return instance;
}

void World::storeObject(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

bool World::removeObject(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::siege::SiegeNpc>> World::getLocalSiegeNpcs(int32_t locationId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> World::getPlayer(std::string_view name) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> World::getPlayer(int32_t objectId) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::VisibleObject> World::findVisibleObject(int32_t objectId) {
	AION_UNPORTED();
}

bool World::isInWorld(int32_t objectId) {
	AION_UNPORTED();
}

runtime::Ptr<WorldMap> World::getWorldMap(int32_t id) {
	AION_UNPORTED();
}

void World::updatePosition(model::gameobjects::VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading) {
	AION_UNPORTED();
}

void World::updatePosition(model::gameobjects::VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading, bool updateKnownList) {
	AION_UNPORTED();
}

bool World::setPosition(model::gameobjects::VisibleObject& object, int32_t mapId, float x, float y, float z, int8_t heading) {
	AION_UNPORTED();
}

bool World::setPosition(runtime::Ptr<model::gameobjects::VisibleObject> object, int32_t mapId, int32_t instance, float x, float y, float z,
	int8_t heading) {
	AION_UNPORTED();
}

runtime::Ref<WorldPosition> World::createPosition(int32_t mapId, float x, float y, float z, int8_t heading, int32_t instanceId) {
	AION_UNPORTED();
}

void World::spawn(runtime::Ptr<model::gameobjects::VisibleObject> object) {
	AION_UNPORTED();
}

void World::despawn(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void World::despawn(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

void World::updateCachedPlayerName(std::string_view oldName, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> World::getAllPlayers() {
	AION_UNPORTED();
}

void World::forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer) {
	AION_UNPORTED();
}

void World::forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world
