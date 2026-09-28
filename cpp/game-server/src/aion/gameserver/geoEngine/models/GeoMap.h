#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/models/fwd.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::geoEngine::models {

/**
 * The geometry of one world map: scene chunks, the terrain and the despawnable nodes (placeables, doors, town objects).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the element type of GeoService's maps. RefCounted (fieldmap K4), created with
 * create(mapId). C++ notes: collision queries return CollisionResults by value; the Vector3f position arguments that Java modifies in place
 * (applyCollisionCheckOffsets' pos, findMovementCollision's origin) are references; the nullable direction is std::optional; Stream<Geometry>
 * is a snapshot vector (§7.2). The constructor reaches Node(String), which sets the CollisionIntention.ALL intentions
 * (collision/CollisionIntentionInfo.h).
 *
 * @author Mr. Poke
 */
class GeoMap : public scene::Node {
	AION_MAKE_REF_FRIEND
public:
	static constexpr float COLLISION_CHECK_Z_OFFSET = 1;

private:
	static constexpr float COLLISION_BOUND_OFFSET = 0.5f;
	static constexpr int32_t NODE_CHUNK_SIZE = 256;

	runtime::Field<runtime::Ref<Terrain>> terrain{};
	runtime::HashMap<int32_t, runtime::Ref<scene::Node>> chunkById{AION_LOCK_CLASS(GeoMap::chunkById)};

	runtime::HashMap<int32_t, runtime::Ref<scene::DespawnableNode>> despawnables{AION_LOCK_CLASS(GeoMap::despawnables)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<scene::DespawnableNode>>>> despawnableTownObjects{
		AION_LOCK_CLASS(GeoMap::despawnableTownObjects)};
	runtime::HashMap<int32_t, runtime::Ref<scene::DespawnableNode>> despawnableHouseDoors{AION_LOCK_CLASS(GeoMap::despawnableHouseDoors)};
	runtime::HashMap<int32_t, runtime::Ref<runtime::Array<runtime::Ref<scene::DespawnableNode>>>> despawnableDoors{
		AION_LOCK_CLASS(GeoMap::despawnableDoors)};
	const int32_t mapId;

protected:
	explicit GeoMap(int32_t mapId);
	~GeoMap() override;

public:
	/** Java: new GeoMap(mapId) */
	static runtime::Ref<GeoMap> create(int32_t mapId);

	int32_t getMapId() const { return mapId; }

	int32_t attachChild(runtime::Ptr<scene::Spatial> child) override;

	bool hasTerrain();

	bool hasTerrainMaterials();

	void setTerrain(runtime::Ptr<Terrain> terrain);

private:
	runtime::Ptr<scene::Node> getOrCreateChunk(scene::Spatial& child);

public:
	int32_t getEntityCount();

	float getZ(float x, float y, float zMax, float zMin, int32_t instanceId);

	float getZ(float x, float y, float zMax, float zMin, int32_t instanceId, bool ignoreSlopingSurface);

	math::Vector3f getClosestCollision(float x, float y, float z, float targetX, float targetY, float targetZ, bool atNearGroundZ, int32_t instanceId,
		int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties);

private:
	void applyCollisionCheckOffsets(math::Vector3f& pos, std::optional<math::Vector3f> direction, int32_t instanceId);

	void applyCollisionCheckOffsets(math::Vector3f& pos, std::optional<math::Vector3f> direction, int32_t instanceId, bool allowNaN);

public:
	math::Vector3f findMovementCollision(math::Vector3f& origin, float targetX, float targetY, int32_t instanceId);

	collision::CollisionResults getCollisions(float x, float y, float z, float targetX, float targetY, float targetZ, int32_t instanceId,
		int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties);

	collision::CollisionResults getCollisions(const math::Vector3f& origin, float targetX, float targetY, float targetZ, int32_t instanceId,
		int8_t intentions, runtime::Ptr<collision::IgnoreProperties> ignoreProperties);

	bool canSee(float x, float y, float z, float targetX, float targetY, float targetZ, int32_t instanceId,
		runtime::Ptr<collision::IgnoreProperties> ignoreProperties);

	int32_t getTerrainMaterialAt(float x, float y, float z, int32_t instanceId);

	void spawnPlaceableObject(int32_t instanceId, int32_t staticId);

	void despawnPlaceableObject(int32_t instanceId, int32_t staticId);

	void updateTownToLevel(int32_t townId, int32_t level);

	void setHouseDoorState(int32_t instanceId, int32_t houseAddress, model::house::HouseDoorState state);

	void setDoorState(int32_t instanceId, int32_t doorId, bool open);

private:
	std::set<int32_t> getIgnorableDoorIds();

public:
	/** Java: Stream<Geometry> (§7.2) */
	std::vector<runtime::Ptr<scene::Geometry>> getGeometries();

private:
	/** Java: private static Stream<Geometry> getGeometries(List<Spatial> spatials) */
	static std::vector<runtime::Ptr<scene::Geometry>> getGeometries(const std::vector<runtime::Ptr<scene::Spatial>>& spatials);
};

} // namespace aion::gameserver::geoEngine::models
