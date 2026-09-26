#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/geoEngine/models/fwd.h"
#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/world/geo/fwd.h"

namespace aion::gameserver::world::geo {

/**
 * Facade of the geo data: heights, line of sight and collisions per world map.
 * <p>
 * Hub header (docs/design/hub-headers.md). An Immortal singleton (fieldmap base Immortal). `geoMaps` is filled once by init() (before other
 * threads read it) and immutable afterwards; the geo models are built eagerly and never pin reclamation (runtime-architecture.md §9).
 *
 * @author ATracer
 */
class GeoService : public runtime::Immortal, public model::GameEngine {
private:
	runtime::HashMap<int32_t, runtime::Ref<geoEngine::models::GeoMap>> geoMaps{AION_LOCK_CLASS(GeoService::geoMaps)};

	GeoService();
	~GeoService() override;

public:
	/** Creates the GeoMaps of WORLD_MAPS_DATA and loads the geo data if GeoDataConfig.GEO_ENABLE. */
	void init() override;

	float getZ(model::gameobjects::VisibleObject& object, float zMax, float zMin);

	float getZ(int32_t worldId, float x, float y, float z, int32_t instanceId);

	float getZ(int32_t worldId, float x, float y, float zMax, float zMin, int32_t instanceId);

	/** @param ignoreProperties may be null */
	geoEngine::collision::CollisionResults getCollisions(model::gameobjects::VisibleObject& object, float x, float y, float z, int8_t intentions,
		runtime::Ptr<geoEngine::collision::IgnoreProperties> ignoreProperties);

	bool canSee(model::gameobjects::VisibleObject& object, model::gameobjects::VisibleObject& target);

	bool canSee(model::gameobjects::VisibleObject& object, float targetX, float targetY, float targetZ,
		geoEngine::collision::IgnoreProperties& ignoreProperties);

private:
	float getSeeCheckOffset(model::gameobjects::VisibleObject& object);

public:
	geoEngine::math::Vector3f getClosestCollision(model::gameobjects::Creature& object, float x, float y, float z);

	geoEngine::math::Vector3f getClosestCollision(model::gameobjects::Creature& object, float x, float y, float z, bool atNearGroundZ,
		int8_t intentions, geoEngine::collision::IgnoreProperties& ignoreProperties);

	geoEngine::math::Vector3f findMovementCollision(model::gameobjects::Creature& creature, float directionAngle, float maxDistance);

private:
	geoEngine::math::Vector3f calculateCurrentGeoPosition(model::gameobjects::player::Player& player);

public:
	void spawnPlaceableObject(int32_t worldId, int32_t instanceId, int32_t staticId);

	void despawnPlaceableObject(int32_t worldId, int32_t instanceId, int32_t staticId);

	void updateTown(model::Race race, int32_t townId, int32_t level);

	void setHouseDoorState(int32_t worldId, int32_t instanceId, int32_t houseAddress, model::house::HouseDoorState state);

	void setDoorState(int32_t worldId, int32_t instanceId, int32_t doorId, bool open);

	bool worldHasTerrainMaterials(int32_t worldId);

	int32_t getTerrainMaterialAt(int32_t worldId, float x, float y, float z, int32_t instanceId);

	static GeoService& getInstance();
};

} // namespace aion::gameserver::world::geo
