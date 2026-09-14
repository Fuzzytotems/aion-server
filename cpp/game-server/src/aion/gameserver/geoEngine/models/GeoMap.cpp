#include "aion/gameserver/geoEngine/models/GeoMap.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/models/Terrain.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"

namespace aion::gameserver::geoEngine::models {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.models.GeoMap");

GeoMap::GeoMap(int32_t mapIdValue) : Node(std::nullopt), mapId(mapIdValue) { // Java: super(null)
}

GeoMap::~GeoMap() = default;

runtime::Ref<GeoMap> GeoMap::create(int32_t mapIdValue) {
	return runtime::makeRef<GeoMap>(mapIdValue);
}

int32_t GeoMap::attachChild(runtime::Ptr<scene::Spatial> child) {
	AION_UNPORTED();
}

bool GeoMap::hasTerrain() {
	AION_UNPORTED();
}

bool GeoMap::hasTerrainMaterials() {
	AION_UNPORTED();
}

void GeoMap::setTerrain(runtime::Ptr<Terrain> value) {
	terrain.set(value);
}

runtime::Ptr<scene::Node> GeoMap::getOrCreateChunk(scene::Spatial& child) {
	AION_UNPORTED();
}

int32_t GeoMap::getEntityCount() {
	AION_UNPORTED();
}

float GeoMap::getZ(float x, float y, float zMax, float zMin, int32_t instanceId) {
	AION_UNPORTED();
}

float GeoMap::getZ(float x, float y, float zMax, float zMin, int32_t instanceId, bool ignoreSlopingSurface) {
	AION_UNPORTED();
}

math::Vector3f GeoMap::getClosestCollision(float x, float y, float z, float targetX, float targetY, float targetZ, bool atNearGroundZ,
	int32_t instanceId, int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	AION_UNPORTED();
}

void GeoMap::applyCollisionCheckOffsets(math::Vector3f& pos, std::optional<math::Vector3f> direction, int32_t instanceId) {
	AION_UNPORTED();
}

void GeoMap::applyCollisionCheckOffsets(math::Vector3f& pos, std::optional<math::Vector3f> direction, int32_t instanceId, bool allowNaN) {
	AION_UNPORTED();
}

math::Vector3f GeoMap::findMovementCollision(math::Vector3f& origin, float targetX, float targetY, int32_t instanceId) {
	AION_UNPORTED();
}

collision::CollisionResults GeoMap::getCollisions(float x, float y, float z, float targetX, float targetY, float targetZ, int32_t instanceId,
	int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	AION_UNPORTED();
}

collision::CollisionResults GeoMap::getCollisions(const math::Vector3f& origin, float targetX, float targetY, float targetZ, int32_t instanceId,
	int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	AION_UNPORTED();
}

bool GeoMap::canSee(float x, float y, float z, float targetX, float targetY, float targetZ, int32_t instanceId,
	runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	AION_UNPORTED();
}

int32_t GeoMap::getTerrainMaterialAt(float x, float y, float z, int32_t instanceId) {
	AION_UNPORTED();
}

void GeoMap::spawnPlaceableObject(int32_t instanceId, int32_t staticId) {
	AION_UNPORTED();
}

void GeoMap::despawnPlaceableObject(int32_t instanceId, int32_t staticId) {
	AION_UNPORTED();
}

void GeoMap::updateTownToLevel(int32_t townId, int32_t level) {
	AION_UNPORTED();
}

void GeoMap::setHouseDoorState(int32_t instanceId, int32_t houseAddress, model::house::HouseDoorState state) {
	AION_UNPORTED();
}

void GeoMap::setDoorState(int32_t instanceId, int32_t doorId, bool open) {
	AION_UNPORTED();
}

std::set<int32_t> GeoMap::getIgnorableDoorIds() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<scene::Geometry>> GeoMap::getGeometries() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<scene::Geometry>> GeoMap::getGeometries(const std::vector<runtime::Ptr<scene::Spatial>>& spatials) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::geoEngine::models
