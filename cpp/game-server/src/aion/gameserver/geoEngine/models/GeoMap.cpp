#include "aion/gameserver/geoEngine/models/GeoMap.h"

#include <algorithm>
#include <array>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/bounding/BoundingVolume.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/math/Vector2f.h"
#include "aion/gameserver/geoEngine/models/Terrain.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/model/house/HouseDoorState.h"
// keep last: Java float semantics for the expressions of this file
#include "aion/gameserver/geoEngine/math/StrictFp.h"

namespace aion::gameserver::geoEngine::models {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.geoEngine.models.GeoMap");

using collision::CollisionIntention;
using collision::CollisionResult;
using collision::CollisionResults;
using math::JavaFloat;
using math::Vector3f;
using scene::DespawnableNode;

namespace {

using DespawnableType = DespawnableNode::DespawnableType;

/** Java: RegionUtil.get2DRegionId(regionSize, x, y) (world/RegionUtil, P4-10, has no C++ header yet) */
int32_t get2DRegionId(int32_t regionSize, float x, float y) {
	constexpr int32_t X_2D_OFFSET = 1000; // RegionUtil.X_2D_OFFSET
	return JavaFloat::doubleToInt(x) / regionSize * X_2D_OFFSET + JavaFloat::doubleToInt(y) / regionSize;
}

/** The ids of the WorldMapType constants (WorldMapType.java; the generated enum has no id companion yet, P4-10). */
constexpr std::array<int32_t, 160> WORLD_MAP_TYPE_IDS{
	120010000, 120020000, 220010000, 220020000, 220030000, 220040000, 220050000, 110010000, 110020000, 210010000, 210020000, 210030000,
	210040000, 210060000, 210050000, 220070000, 600010000, 510010000, 520010000, 400010000, 300010000, 300020000, 300030000, 300040000,
	300050000, 300060000, 300070000, 300080000, 300090000, 300100000, 300110000, 300120000, 300130000, 300140000, 300150000, 300160000,
	300170000, 300190000, 300200000, 300210000, 300220000, 300230000, 310010000, 310020000, 310030000, 310040000, 310050000, 310060000,
	310070000, 310080000, 320090000, 310090000, 310100000, 310110000, 310120000, 320010000, 320020000, 320030000, 320040000, 320050000,
	320060000, 320070000, 320080000, 320100000, 320110000, 320120000, 320130000, 320140000, 110070000, 120080000, 300250000, 300300000,
	300320000, 300350000, 300360000, 300420000, 300430000, 320150000, 900020000, 900030000, 900100000, 900110000, 900120000, 900130000,
	900140000, 900150000, 900170000, 900180000, 900190000, 900200000, 900220000, 300310000, 300280000, 300240000, 300460000, 300440000,
	300450000, 300510000, 300520000, 300550000, 300560000, 300570000, 300600000, 300700000, 300480000, 300540000, 300590000, 300800000,
	301110000, 301120000, 301130000, 301140000, 301160000, 301200000, 600080000, 301210000, 301220000, 301230000, 301240000, 301250000,
	301260000, 301270000, 301280000, 301290000, 301300000, 301310000, 301320000, 301330000, 301340000, 301360000, 301370000, 400020000,
	400030000, 400040000, 400050000, 400060000, 600090000, 600100000, 700010000, 710010000, 300290000, 301400000, 130090000, 140010000,
	220100000, 210070000, 210080000, 210090000, 220080000, 220090000, 300610000, 300620000, 300630000, 301380000, 301390000, 301500000,
	700020000, 710020000, 720010000, 730010000,
};

} // namespace

GeoMap::GeoMap(int32_t mapIdValue) : Node(std::nullopt), mapId(mapIdValue) { // Java: super(null)
}

GeoMap::~GeoMap() = default;

runtime::Ref<GeoMap> GeoMap::create(int32_t mapIdValue) {
	return runtime::makeRef<GeoMap>(mapIdValue);
}

int32_t GeoMap::attachChild(runtime::Ptr<scene::Spatial> child) {
	if (runtime::Ptr<DespawnableNode> desp = runtime::as<DespawnableNode>(child)) {
		switch (desp->type.get()) {
			case DespawnableType::EVENT: // event object
				break;
			case DespawnableType::PLACEABLE: // placeable
				despawnables.put(desp->id.get(), runtime::Ref<DespawnableNode>(desp));
				break;
			case DespawnableType::HOUSE: // house
				break;
			case DespawnableType::HOUSE_DOOR: // house door
				despawnableHouseDoors.put(desp->id.get(), runtime::Ref<DespawnableNode>(desp));
				break;
			case DespawnableType::TOWN_OBJECT: // town object
				despawnableTownObjects
					.computeIfAbsent(desp->id.get(), [] { return runtime::RcArrayList<runtime::Ref<DespawnableNode>>::create(); })
					->add(runtime::Ref<DespawnableNode>(desp));
				break;
			case DespawnableType::DOOR_STATE1: // normal door state 1 (closed)
			case DespawnableType::DOOR_STATE2: { // normal door state 2 (opened)
				runtime::Ptr<runtime::Array<runtime::Ref<DespawnableNode>>> doorStates =
					despawnableDoors.computeIfAbsent(desp->id.get(), [] { return runtime::Array<runtime::Ref<DespawnableNode>>::make(2); });
				(*doorStates)[desp->type.get() == DespawnableType::DOOR_STATE1 ? 0 : 1].set(desp);
				break;
			}
			default:
				throw runtime::IllegalArgumentException(std::string(xml::enumName(desp->type.get())) + " is not implemented");
		}
	}
	getOrCreateChunk(*child)->attachChild(child);
	return 0;
}

bool GeoMap::hasTerrain() {
	return terrain.get() != nullptr;
}

bool GeoMap::hasTerrainMaterials() {
	runtime::Ptr<Terrain> t = terrain.get();
	return t != nullptr && t->hasMaterials();
}

void GeoMap::setTerrain(runtime::Ptr<Terrain> value) {
	terrain.set(value);
}

runtime::Ptr<scene::Node> GeoMap::getOrCreateChunk(scene::Spatial& child) {
	Vector3f center = child.getWorldBound()->getCenter();
	int32_t chunkId = get2DRegionId(NODE_CHUNK_SIZE, center.x, center.y);
	runtime::Ptr<scene::Node> node = chunkById.get(chunkId);
	if (node == nullptr) {
		runtime::Ref<scene::Node> created = scene::Node::create(std::string_view(""));
		node = created;
		chunkById.put(chunkId, created);
		Node::attachChild(node);
	}
	return node;
}

int32_t GeoMap::getEntityCount() {
	int32_t entities = 0;
	for (const auto& entry : chunkById.snapshot())
		entities += entry.value->getChildren()->size();
	return entities;
}

float GeoMap::getZ(float x, float y, float zMax, float zMin, int32_t instanceId) {
	return getZ(x, y, zMax, zMin, instanceId, false);
}

float GeoMap::getZ(float x, float y, float zMax, float zMin, int32_t instanceId, bool ignoreSlopingSurface) {
	CollisionResults results(collision::getId(CollisionIntention::PHYSICAL), instanceId);
	results.setInvalidateSlopingSurface(ignoreSlopingSurface);
	Vector3f origin(x, y, zMax);
	Vector3f target(x, y, zMin);
	target.subtractLocal(origin).normalizeLocal(); // convert to direction vector
	math::Ray r(origin, target);
	r.setLimit(zMax - zMin);
	collideWith(r, results);
	if (runtime::Ptr<Terrain> t = terrain.get())
		t->collideAtOrigin(r, results);
	std::optional<CollisionResult> closestCollision = results.getClosestCollision();
	return !closestCollision ? std::numeric_limits<float>::quiet_NaN() : closestCollision->getContactPoint().z;
}

Vector3f GeoMap::getClosestCollision(float x, float y, float z, float targetX, float targetY, float targetZ, bool atNearGroundZ, int32_t instanceId,
	int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	Vector3f origin(x, y, z + COLLISION_CHECK_Z_OFFSET);
	std::optional<CollisionResult> closestCollision =
		getCollisions(origin, targetX, targetY, targetZ + COLLISION_CHECK_Z_OFFSET, instanceId, intentions, ignoreProperties).getClosestCollision();
	if (!closestCollision) {
		Vector3f end(targetX, targetY, targetZ);
		if (atNearGroundZ) {
			float geoZ = getZ(end.x, end.y, end.z + 1, end.z - 2, instanceId);
			if (!JavaFloat::isNaN(geoZ))
				end.z = geoZ;
		}
		return end;
	} else if (closestCollision->getDistance() <= COLLISION_BOUND_OFFSET + 0.05f) { // avoid climbing steep hills or passing through walls
		return Vector3f(x, y, z);
	}
	Vector3f contactPoint = closestCollision->getContactPoint();
	applyCollisionCheckOffsets(contactPoint, origin, instanceId);
	return contactPoint;
}

void GeoMap::applyCollisionCheckOffsets(Vector3f& pos, std::optional<Vector3f> direction, int32_t instanceId) {
	applyCollisionCheckOffsets(pos, direction, instanceId, false);
}

void GeoMap::applyCollisionCheckOffsets(Vector3f& pos, std::optional<Vector3f> direction, int32_t instanceId, bool allowNaN) {
	if (direction) {
		Vector3f dir = pos.subtract(*direction).normalizeLocal();
		pos.subtractLocal(dir.multLocal(COLLISION_BOUND_OFFSET)); // set contact point back for proper ground calculation
		float geoZ = getZ(pos.x, pos.y, pos.z, pos.z - COLLISION_CHECK_Z_OFFSET * 3, instanceId);
		if (allowNaN || !JavaFloat::isNaN(geoZ)) {
			pos.z = geoZ;
		} else {
			pos.z -= COLLISION_CHECK_Z_OFFSET;
		}
	} else {
		pos.z -= COLLISION_CHECK_Z_OFFSET;
	}
}

Vector3f GeoMap::findMovementCollision(Vector3f& origin, float targetX, float targetY, int32_t instanceId) {
	// check if we have an obstacle 1m in target direction
	origin.setZ(origin.getZ() + COLLISION_CHECK_Z_OFFSET);
	math::Vector2f targetXY(targetX, targetY);
	math::Vector2f xyOffset = targetXY.subtract(origin.getX(), origin.getY());
	xyOffset.normalizeLocal().multLocal(COLLISION_CHECK_Z_OFFSET);
	float nextX = origin.getX() + xyOffset.getX(), nextY = origin.getY() + xyOffset.getY();
	if ((xyOffset.getX() >= 0 && nextX > targetX) || (xyOffset.getX() < 0 && nextX < targetX))
		nextX = targetX;
	if ((xyOffset.getY() >= 0 && nextY > targetY) || (xyOffset.getY() < 0 && nextY < targetY))
		nextY = targetY;
	if (origin.getX() != nextX || origin.getY() != nextY) {
		std::optional<CollisionResult> closestCollision = getCollisions(origin, nextX, nextY, origin.getZ(), instanceId,
			collision::getId(CollisionIntention::DEFAULT_COLLISIONS), collision::IgnoreProperties::ANY_RACE)
																.getClosestCollision();
		if (closestCollision) { // obstacle found within 1m in target direction, return 0.5m offset position or origin of there's no ground
			Vector3f targetPoint = closestCollision->getContactPoint();
			applyCollisionCheckOffsets(targetPoint, origin, instanceId, true);
			if (!JavaFloat::isNaN(targetPoint.getZ()))
				return targetPoint;
		} else { // no obstacle 1m in target direction, now check if there's ground to stand on
			float geoZ = getZ(nextX, nextY, origin.getZ(), origin.getZ() - COLLISION_CHECK_Z_OFFSET * 2.5f, instanceId, true);
			if (!JavaFloat::isNaN(geoZ)) // there is ground, so we set our origin to the 1m offset position and start over
				return findMovementCollision(origin.set(nextX, nextY, geoZ), targetX, targetY, instanceId);
		}
	}
	return origin.setZ(origin.getZ() - COLLISION_CHECK_Z_OFFSET);
}

CollisionResults GeoMap::getCollisions(float x, float y, float z, float targetX, float targetY, float targetZ, int32_t instanceId, int8_t intentions,
	runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	return getCollisions(Vector3f(x, y, z), targetX, targetY, targetZ, instanceId, intentions, ignoreProperties);
}

CollisionResults GeoMap::getCollisions(const Vector3f& origin, float targetX, float targetY, float targetZ, int32_t instanceId, int8_t intentions,
	runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	CollisionResults results(intentions, instanceId, ignoreProperties);
	Vector3f target(targetX, targetY, targetZ);
	float limit = origin.distance(target);
	target.subtractLocal(origin).normalizeLocal(); // convert to direction vector
	math::Ray r(origin, target);
	r.setLimit(limit);
	if (runtime::Ptr<Terrain> t = terrain.get()) {
		t->collide(r, targetX, targetY, &results);
	}
	collideWith(r, results);
	return results;
}

bool GeoMap::canSee(float x, float y, float z, float targetX, float targetY, float targetZ, int32_t instanceId,
	runtime::Ptr<collision::IgnoreProperties> ignoreProperties) {
	Vector3f origin(x, y, z);
	Vector3f target(targetX, targetY, targetZ);
	float distance = origin.distance(target);
	if (distance > 80.0f)
		return false;
	target.subtractLocal(origin).normalizeLocal(); // convert to direction vector
	math::Ray ray(origin, target);
	ray.setLimit(distance);
	runtime::Ptr<Terrain> t = terrain.get();
	if (t != nullptr && t->collide(ray, targetX, targetY, nullptr))
		return false;
	CollisionResults results(collision::getId(CollisionIntention::CANT_SEE_COLLISIONS), instanceId, true, ignoreProperties);
	return collideWith(ray, results) == 0;
}

int32_t GeoMap::getTerrainMaterialAt(float x, float y, float z, int32_t instanceId) {
	runtime::Ptr<Terrain> t = terrain.get();
	int32_t matId = t == nullptr ? 0 : t->getTerrainMaterialAt(x, y);
	if (matId > 0) {
		CollisionResults results(collision::getId(CollisionIntention::PHYSICAL), instanceId);
		float zMax = z + 1;
		float zMin = z - 1;
		Vector3f origin(x, y, zMax);
		Vector3f target(x, y, zMin);
		target.subtractLocal(origin).normalizeLocal(); // convert to direction vector
		math::Ray r(origin, target);
		r.setLimit(zMax - zMin);
		t->collideAtOrigin(r, results);
		std::optional<CollisionResult> terrainCollision = results.getClosestCollision();
		// Java: results.getClosestCollision().equals(terrainCollision), where the results hold the terrain collision object itself: equals is
		// true through identity when the terrain collision stays the closest one. The sort is stable and the terrain collision was added first,
		// so it stays the closest exactly when no later collision compares smaller (a smaller distance differs, so Java's equals is false).
		if (terrainCollision && (collideWith(r, results) == 0 || results.getClosestCollision()->compareTo(*terrainCollision) == 0)) {
			return matId;
		}
	}
	return 0;
}

void GeoMap::spawnPlaceableObject(int32_t instanceId, int32_t staticId) {
	runtime::Ptr<DespawnableNode> node = despawnables.get(staticId);
	if (node != nullptr) {
		node->setActive(instanceId, true);
	}
}

void GeoMap::despawnPlaceableObject(int32_t instanceId, int32_t staticId) {
	runtime::Ptr<DespawnableNode> node = despawnables.get(staticId);
	if (node != nullptr) {
		node->setActive(instanceId, false);
	}
}

void GeoMap::updateTownToLevel(int32_t townId, int32_t level) {
	if (despawnableTownObjects.containsKey(townId) && !despawnableTownObjects.get(townId)->isEmpty()) {
		for (runtime::Ptr<DespawnableNode> despawnableNode : *despawnableTownObjects.get(townId)) {
			int32_t levelBitMask = static_cast<int32_t>(1u << ((level - 1) & 31)); // Java int shift masks the count
			despawnableNode->setActive(1, (despawnableNode->levelBitMask.get() & levelBitMask) != 0);
		}
	}
}

void GeoMap::setHouseDoorState(int32_t instanceId, int32_t houseAddress, model::house::HouseDoorState state) {
	runtime::Ptr<DespawnableNode> houseDoor = despawnableHouseDoors.get(houseAddress);
	if (houseDoor != nullptr)
		houseDoor->setActive(instanceId, state != model::house::HouseDoorState::OPEN);
}

void GeoMap::setDoorState(int32_t instanceId, int32_t doorId, bool open) {
	runtime::Ptr<runtime::Array<runtime::Ref<DespawnableNode>>> doors = despawnableDoors.get(doorId);
	if (doors == nullptr) {
		if (configs::main::GeoDataConfig::GEO_ENABLE.load() && !getIgnorableDoorIds().contains(doorId))
			log.warn("No geometry found for door " + std::to_string(doorId) + " in world " + std::to_string(mapId));
	} else {
		if (runtime::Ptr<DespawnableNode> closed = doors->get(0)) {
			closed->setActive(instanceId, !open);
		} else {
			log.warn("Door state 1 not available for door " + std::to_string(doorId) + " in world " + std::to_string(mapId));
		}
		if (runtime::Ptr<DespawnableNode> opened = doors->get(1)) {
			opened->setActive(instanceId, open);
		} else {
			log.warn("Door state 2 not available for door " + std::to_string(doorId) + " in world " + std::to_string(mapId));
		}
	}
}

std::set<int32_t> GeoMap::getIgnorableDoorIds() {
	// Java: switch (WorldMapType.getWorld(mapId)) - a map id without a WorldMapType constant makes the switch throw NullPointerException
	if (std::find(WORLD_MAP_TYPE_IDS.begin(), WORLD_MAP_TYPE_IDS.end(), mapId) == WORLD_MAP_TYPE_IDS.end())
		throw runtime::NullPointerException("WorldMapType.getWorld(" + std::to_string(mapId) + ") is null");
	switch (mapId) {
		// TODO mesh is excluded on purpose in geobuilder due to incorrect collision data: objects/npc/level_object/idyun_bridge/idyun_bridge_01a.cga
		case 300280000: // RENTUS_BASE
		case 300620000: // OCCUPIED_RENTUS_BASE
			return {145};
		// all of the following doors have no collision mesh in the game client (you can walk right through them)
		case 300220000: // ABYSSAL_SPLINTER
		case 300600000: // UNSTABLE_SPLINTER
			return {15, 16, 18, 69};
		case 300240000: // ATURAM_SKY_FORTRESS
			return {128, 138, 308, 307};
		case 300250000: // ESOTERRACE
			return {78};
		case 300290000: // Test_MRT_IDZone
			return {49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 73};
		case 300610000: // RAKSANG_RUINS
			return {219};
		case 301120000: // KAMAR_BATTLEFIELD
			return {5, 144};
		default:
			return {};
	}
}

std::vector<runtime::Ptr<scene::Geometry>> GeoMap::getGeometries() {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<scene::Spatial>>> list = getChildren();
	return getGeometries(list->snapshot());
}

std::vector<runtime::Ptr<scene::Geometry>> GeoMap::getGeometries(const std::vector<runtime::Ptr<scene::Spatial>>& spatials) {
	std::vector<runtime::Ptr<scene::Geometry>> geometries;
	for (const runtime::Ptr<scene::Spatial>& child : spatials) {
		if (runtime::Ptr<scene::Geometry> geometry = runtime::as<scene::Geometry>(child)) {
			geometries.push_back(geometry);
		} else if (runtime::Ptr<scene::Node> node = runtime::as<scene::Node>(child)) {
			std::vector<runtime::Ptr<scene::Geometry>> nested = getGeometries(node->getChildren()->snapshot());
			geometries.insert(geometries.end(), nested.begin(), nested.end());
		}
	}
	return geometries;
}

} // namespace aion::gameserver::geoEngine::models
