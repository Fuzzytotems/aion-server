#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::world::geo {

GeoService::GeoService() = default;

GeoService::~GeoService() = default;

GeoService& GeoService::getInstance() {
	static GeoService instance; // Java: SingletonHolder
	return instance;
}

void GeoService::init() {
	// Java: a logger created in the body, LoggerFactory.getLogger(GeoService.class).warn("Geo data is disabled")
	AION_UNPORTED();
}

float GeoService::getZ(model::gameobjects::VisibleObject& object, float zMax, float zMin) {
	AION_UNPORTED();
}

float GeoService::getZ(int32_t worldId, float x, float y, float z, int32_t instanceId) {
	AION_UNPORTED();
}

float GeoService::getZ(int32_t worldId, float x, float y, float zMax, float zMin, int32_t instanceId) {
	AION_UNPORTED();
}

geoEngine::collision::CollisionResults GeoService::getCollisions(model::gameobjects::VisibleObject& object, float x, float y, float z,
	int8_t intentions, runtime::Ptr<geoEngine::collision::IgnoreProperties> ignoreProperties) {
	AION_UNPORTED();
}

bool GeoService::canSee(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target) {
	AION_UNPORTED();
}

bool GeoService::canSee(model::gameobjects::VisibleObject& object, float targetX, float targetY, float targetZ,
	geoEngine::collision::IgnoreProperties& ignoreProperties) {
	AION_UNPORTED();
}

float GeoService::getSeeCheckOffset(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

geoEngine::math::Vector3f GeoService::getClosestCollision(model::gameobjects::Creature& object, float x, float y, float z) {
	AION_UNPORTED();
}

geoEngine::math::Vector3f GeoService::getClosestCollision(model::gameobjects::Creature& object, float x, float y, float z, bool atNearGroundZ,
	int8_t intentions, geoEngine::collision::IgnoreProperties& ignoreProperties) {
	AION_UNPORTED();
}

geoEngine::math::Vector3f GeoService::findMovementCollision(model::gameobjects::Creature& creature, float directionAngle, float maxDistance) {
	AION_UNPORTED();
}

geoEngine::math::Vector3f GeoService::calculateCurrentGeoPosition(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void GeoService::spawnPlaceableObject(int32_t worldId, int32_t instanceId, int32_t staticId) {
	AION_UNPORTED();
}

void GeoService::despawnPlaceableObject(int32_t worldId, int32_t instanceId, int32_t staticId) {
	AION_UNPORTED();
}

void GeoService::updateTown(model::Race race, int32_t townId, int32_t level) {
	AION_UNPORTED();
}

void GeoService::setHouseDoorState(int32_t worldId, int32_t instanceId, int32_t houseAddress, model::house::HouseDoorState state) {
	AION_UNPORTED();
}

void GeoService::setDoorState(int32_t worldId, int32_t instanceId, int32_t doorId, bool open) {
	AION_UNPORTED();
}

bool GeoService::worldHasTerrainMaterials(int32_t worldId) {
	AION_UNPORTED();
}

int32_t GeoService::getTerrainMaterialAt(int32_t worldId, float x, float y, float z, int32_t instanceId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::geo
