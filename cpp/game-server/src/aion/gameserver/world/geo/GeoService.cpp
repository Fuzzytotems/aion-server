#include "aion/gameserver/world/geo/GeoService.h"

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/GeoWorldLoader.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"

namespace aion::gameserver::world::geo {

namespace {

using geoEngine::collision::CollisionIntention;
using geoEngine::collision::IgnoreProperties;
using geoEngine::math::Vector3f;
using model::gameobjects::VisibleObject;

/** Java: Math.toRadians(angdeg) = angdeg * DEGREES_TO_RADIANS */
double toRadians(double angdeg) {
	constexpr double DEGREES_TO_RADIANS = 0.017453292519943295;
	return angdeg * DEGREES_TO_RADIANS;
}

/**
 * Java: GeoWorldLoader.createZone -> ZoneService.getInstance().createMaterialZoneTemplate(geometry, worldId, ZoneName.createOrGet(zoneName)); the
 * geo engine reaches it through GeoCallbacks (P4-04), registered by init before the geo data is loaded.
 */
void createMaterialZone(geoEngine::scene::Spatial& geometry, int32_t worldId, std::string_view zoneName) {
	zone::ZoneService::getInstance().createMaterialZoneTemplate(geometry, worldId, zone::ZoneName::createOrGet(zoneName));
}

/** Java: geoMaps.get(worldId), NullPointerException on the call for a map without geo map */
geoEngine::models::GeoMap& geoMapOf(runtime::HashMap<int32_t, runtime::Ref<geoEngine::models::GeoMap>>& geoMaps, int32_t worldId) {
	runtime::Ptr<geoEngine::models::GeoMap> map = geoMaps.get(worldId);
	if (!map)
		throw runtime::NullPointerException("GeoService: no GeoMap for world " + std::to_string(worldId));
	return *map;
}

} // namespace

GeoService::GeoService() = default;

GeoService::~GeoService() = default;

GeoService& GeoService::getInstance() {
	static GeoService instance; // Java: SingletonHolder
	return instance;
}

void GeoService::init() {
	for (const model::templates::world::WorldMapTemplate* map : *dataholders::DataManager::WORLD_MAPS_DATA)
		geoMaps.put(map->getMapId(), geoEngine::models::GeoMap::create(map->getMapId()));
	if (configs::main::GeoDataConfig::GEO_ENABLE.load()) {
		geoEngine::GeoCallbacks::setMaterialZoneSink(&createMaterialZone);
		std::vector<runtime::Ptr<geoEngine::models::GeoMap>> maps = geoMaps.values().toVector();
		geoEngine::GeoWorldLoader::load(maps);
	} else {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.geo.GeoService").warn("Geo data is disabled");
	}
}

float GeoService::getZ(VisibleObject& object, float zMax, float zMin) {
	return getZ(object.getWorldId(), object.getX(), object.getY(), zMax, zMin, object.getInstanceId());
}

float GeoService::getZ(int32_t worldId, float x, float y, float z, int32_t instanceId) {
	return getZ(worldId, x, y, z + 2, z - 2, instanceId);
}

float GeoService::getZ(int32_t worldId, float x, float y, float zMax, float zMin, int32_t instanceId) {
	return geoMapOf(geoMaps, worldId).getZ(x, y, zMax, zMin, instanceId);
}

geoEngine::collision::CollisionResults GeoService::getCollisions(VisibleObject& object, float x, float y, float z, int8_t intentions,
	runtime::Ptr<IgnoreProperties> ignoreProperties) {
	return geoMapOf(geoMaps, object.getWorldId())
		.getCollisions(object.getX(), object.getY(), object.getZ() + getSeeCheckOffset(object), x, y, z, object.getInstanceId(), intentions,
			ignoreProperties);
}

bool GeoService::canSee(VisibleObject& object, VisibleObject& target) {
	if (!configs::main::GeoDataConfig::CANSEE_ENABLE.load())
		return true;

	float objectSeeCheckZ = object.getZ() + getSeeCheckOffset(object);
	float targetSeeCheckZ = target.getZ() + getSeeCheckOffset(target);
	float x = object.getX();
	float y = object.getY();
	float targetX = target.getX();
	float targetY = target.getY();
	if (auto* npc = dynamic_cast<model::gameobjects::Npc*>(&object);
		npc != nullptr && npc->getAi().ask(ai::poll::AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKING)) {
		double rad = toRadians(utils::PositionUtil::calculateAngleFrom(object, target));
		x += static_cast<float>(std::cos(rad) * object.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide());
		y += static_cast<float>(std::sin(rad) * object.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide());
	}
	if (auto* npc = dynamic_cast<model::gameobjects::Npc*>(&target);
		npc != nullptr && npc->getAi().ask(ai::poll::AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKED)) {
		double rad = toRadians(utils::PositionUtil::calculateAngleFrom(target, object));
		targetX += static_cast<float>(std::cos(rad) * target.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide());
		targetY += static_cast<float>(std::sin(rad) * target.getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide());
	}
	std::optional<model::Race> race; // Java: Race race = null
	int32_t staticId = -1;
	if (target.getSpawn()) {
		staticId = target.getSpawn()->getStaticId();
	}
	if (auto* creature = dynamic_cast<model::gameobjects::Creature*>(&object)) {
		race = creature->getRace();
	}
	runtime::Ref<IgnoreProperties> ignoreProperties = IgnoreProperties::of(race, staticId);
	return geoMapOf(geoMaps, object.getWorldId())
		.canSee(x, y, objectSeeCheckZ, targetX, targetY, targetSeeCheckZ, object.getInstanceId(), ignoreProperties);
}

bool GeoService::canSee(VisibleObject& object, float targetX, float targetY, float targetZ, IgnoreProperties& ignoreProperties) {
	float zOffset = getSeeCheckOffset(object);
	return geoMapOf(geoMaps, object.getWorldId())
		.canSee(object.getX(), object.getY(), object.getZ() + zOffset, targetX, targetY, targetZ + zOffset, object.getInstanceId(),
			runtime::Ptr<IgnoreProperties>(ignoreProperties));
}

float GeoService::getSeeCheckOffset(VisibleObject& object) {
	float height = object.getObjectTemplate()->getBoundRadius()->getUpper();
	if (auto* p = dynamic_cast<model::gameobjects::player::Player*>(&object); p != nullptr && p->isTransformed() && p->getTransformModel().cantMove()) {
		const model::templates::npc::NpcTemplate* t = dataholders::DataManager::NPC_DATA->getNpcTemplate(p->getTransformModel().getModelId());
		if (t != nullptr)
			return t->getBoundRadius()->getUpper();
	}
	return height > 2.5f ? height / 2 : 1.25f;
}

Vector3f GeoService::getClosestCollision(model::gameobjects::Creature& object, float x, float y, float z) {
	return getClosestCollision(object, x, y, z, true, getId(CollisionIntention::DEFAULT_COLLISIONS), *IgnoreProperties::ANY_RACE);
}

Vector3f GeoService::getClosestCollision(model::gameobjects::Creature& object, float x, float y, float z, bool atNearGroundZ, int8_t intentions,
	IgnoreProperties& ignoreProperties) {
	return geoMapOf(geoMaps, object.getWorldId())
		.getClosestCollision(object.getX(), object.getY(), object.getZ(), x, y, z, atNearGroundZ, object.getInstanceId(), intentions,
			runtime::Ptr<IgnoreProperties>(ignoreProperties));
}

Vector3f GeoService::findMovementCollision(model::gameobjects::Creature& creature, float directionAngle, float maxDistance) {
	double rad = toRadians(directionAngle);
	float x1 = static_cast<float>(std::cos(rad) * maxDistance);
	float y1 = static_cast<float>(std::sin(rad) * maxDistance);
	Vector3f startPos;
	geoEngine::models::GeoMap& map = geoMapOf(geoMaps, creature.getWorldId());
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature)) {
		startPos = calculateCurrentGeoPosition(*player);
		if (creature.isFlying())
			return map.getClosestCollision(startPos.getX(), startPos.getY(), startPos.getZ(), startPos.getX() + x1, startPos.getY() + y1,
				startPos.getZ(), false, creature.getInstanceId(), getId(CollisionIntention::DEFAULT_COLLISIONS), IgnoreProperties::ANY_RACE);
	} else
		startPos = Vector3f(creature.getX(), creature.getY(), creature.getZ());
	return map.findMovementCollision(startPos, startPos.getX() + x1, startPos.getY() + y1, creature.getInstanceId());
}

Vector3f GeoService::calculateCurrentGeoPosition(model::gameobjects::player::Player& player) {
	runtime::Ptr<WorldPosition> approximatePos = player.getPosition();
	runtime::Ptr<WorldPosition> lastPos = player.getMoveController()->getLastPositionFromClient();
	if (!lastPos)
		return Vector3f(approximatePos->getX(), approximatePos->getY(), approximatePos->getZ());
	// client sends CM_MOVE in intervals when moving straight, so we search for possible collisions between lastPos and the server side position
	return geoMapOf(geoMaps, approximatePos->getMapId())
		.getClosestCollision(lastPos->getX(), lastPos->getY(), lastPos->getZ(), approximatePos->getX(), approximatePos->getY(),
			approximatePos->getZ(), true, approximatePos->getInstanceId(), getId(CollisionIntention::DEFAULT_COLLISIONS), IgnoreProperties::ANY_RACE);
}

void GeoService::spawnPlaceableObject(int32_t worldId, int32_t instanceId, int32_t staticId) {
	geoMapOf(geoMaps, worldId).spawnPlaceableObject(instanceId, staticId);
}

void GeoService::despawnPlaceableObject(int32_t worldId, int32_t instanceId, int32_t staticId) {
	geoMapOf(geoMaps, worldId).despawnPlaceableObject(instanceId, staticId);
}

void GeoService::updateTown(model::Race race, int32_t townId, int32_t level) {
	switch (race) {
		case model::Race::ELYOS:
			geoMapOf(geoMaps, getId(WorldMapType::ORIEL)).updateTownToLevel(townId, level);
			break;
		case model::Race::ASMODIANS:
			geoMapOf(geoMaps, getId(WorldMapType::PERNON)).updateTownToLevel(townId, level);
			break;
		default:
			break;
	}
}

void GeoService::setHouseDoorState(int32_t worldId, int32_t instanceId, int32_t houseAddress, model::house::HouseDoorState state) {
	geoMapOf(geoMaps, worldId).setHouseDoorState(instanceId, houseAddress, state);
}

void GeoService::setDoorState(int32_t worldId, int32_t instanceId, int32_t doorId, bool open) {
	geoMapOf(geoMaps, worldId).setDoorState(instanceId, doorId, open);
}

bool GeoService::worldHasTerrainMaterials(int32_t worldId) {
	return configs::main::GeoDataConfig::GEO_MATERIALS_ENABLE.load() && geoMapOf(geoMaps, worldId).hasTerrainMaterials();
}

int32_t GeoService::getTerrainMaterialAt(int32_t worldId, float x, float y, float z, int32_t instanceId) {
	return configs::main::GeoDataConfig::GEO_MATERIALS_ENABLE.load() ? geoMapOf(geoMaps, worldId).getTerrainMaterialAt(x, y, z, instanceId) : 0;
}

} // namespace aion::gameserver::world::geo
