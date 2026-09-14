#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/runtime/base/Unported.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor, the destructor and getInstance() (which defines the instance) need the
// complete GeoMap (not a hub; its header comes with the geo chunk P4-04). Remove the guard once it exists.
#if __has_include("aion/gameserver/geoEngine/models/GeoMap.h")
#define AION_S0B_GEO_SERVICE_MEMBERS 1
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#else
#define AION_S0B_GEO_SERVICE_MEMBERS 0
#endif
// S0b transition: getCollisions returns CollisionResults by value (not a hub; P4-04). Remove the guard once its header exists.
#if __has_include("aion/gameserver/geoEngine/collision/CollisionResults.h")
#define AION_S0B_GEO_SERVICE_COLLISION_RESULTS 1
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#else
#define AION_S0B_GEO_SERVICE_COLLISION_RESULTS 0
#endif

namespace aion::gameserver::world::geo {

#if AION_S0B_GEO_SERVICE_MEMBERS
GeoService::GeoService() = default;

GeoService::~GeoService() = default;

GeoService& GeoService::getInstance() {
	static GeoService instance; // Java: SingletonHolder
	return instance;
}
#endif

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

#if AION_S0B_GEO_SERVICE_COLLISION_RESULTS
geoEngine::collision::CollisionResults GeoService::getCollisions(model::gameobjects::VisibleObject& object, float x, float y, float z,
	int8_t intentions, runtime::Ptr<geoEngine::collision::IgnoreProperties> ignoreProperties) {
	AION_UNPORTED();
}
#endif

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
