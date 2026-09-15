#include "aion/gameserver/world/World.h"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/ShieldService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/collections/CollectionUtil.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/container/PlayerContainer.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/world/exceptions/AlreadySpawnedException.h"
#include "aion/gameserver/world/exceptions/DuplicateAionObjectException.h"
#include "aion/commons/utils/WindowsMacroGuard.h"

namespace aion::gameserver::world {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.world.World");

namespace {

using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::gameobjects::siege::SiegeNpc;

using geoEngine::math::JavaFloat;

/** Java: `new Throwable()` / `new Exception()` handed to a log call only for its stack trace */
runtime::Exception stackTrace(const char* javaClass) {
	return runtime::Exception(javaClass);
}

} // namespace

World::World() : allPlayers(container::PlayerContainer::create()) {
	// Java: DataManager.WORLD_MAPS_DATA.forEachParalllel(...). Performance (M4, the same maps and puts): every map is created in its own task
	// scope (PER_ELEMENT), so the Reclaimer frees what one map's creation retired (the Point3D of the 3D region/zone tests, replaced neighbour
	// arrays) while the others are created. Each element opens a QuiescentScope (its frames hold no borrow) and the World creation opt-in, under
	// which the long loops of WorldMap/WorldMap3DInstance place quiescent points. A caller that opened a QuiescentScope and the opt-in (main's
	// world step) is unpublished before it waits; any other caller gets the same maps without quiescent points on its own thread.
	auto createMap = [this](const model::templates::world::WorldMapTemplate* worldMapTemplate) {
		runtime::Ref<WorldMap> worldMap = WorldMap::create(worldMapTemplate);
		SYNCHRONIZED(worldMaps.monitor()) {
			worldMaps.put(worldMapTemplate->getMapId(), worldMap);
		}
	};
	std::vector<const model::templates::world::WorldMapTemplate*> templates(dataholders::DataManager::WORLD_MAPS_DATA->begin(),
		dataholders::DataManager::WORLD_MAPS_DATA->end());
	bool quiescentCaller = runtime::QuiescentOptIn::active(runtime::QuiescentOptIn::WORLD_CREATION);
	if (quiescentCaller && runtime::ForkJoinPool::commonPool().isParallelFromCurrentThread()) {
		// the 3D map first, on this thread (WorldMapInstanceFactory: RESHANTA gets a WorldMap3DInstance): its region loop forks by itself, which
		// an element on a ForkJoin helper could not
		int32_t reshanta = getId(WorldMapType::RESHANTA);
		auto first = std::find_if(templates.begin(), templates.end(), [reshanta](const auto* t) { return t->getMapId() == reshanta; });
		if (first != templates.end()) {
			// quiescent-safe: this frame holds template pointers only, the caller's QuiescentScope and opt-in vouch for the frames above
			runtime::quiescentPoint();
			createMap(*first);
			templates.erase(first);
		}
	}
	if (quiescentCaller)
		runtime::quiescentPoint(); // quiescent-safe: as above; this thread only waits for the elements below
	runtime::ForkJoinPool::commonPool().parallelForEach(
		templates,
		[&createMap](const model::templates::world::WorldMapTemplate* worldMapTemplate) {
			// the element's own task scope on a helper (nested and serial elements run at depth 2, where the scope is inactive)
			runtime::QuiescentScope quiescent; // quiescent-safe: the element's frames hold only the template pointer
			runtime::QuiescentOptIn worldCreation(runtime::QuiescentOptIn::WORLD_CREATION); // vouches for WorldMap::create and below
			createMap(worldMapTemplate);
		},
		runtime::Isolation::PER_ELEMENT);
	services::ShieldService::getInstance().logDetachedShields();
	log.info("World: " + std::to_string(worldMaps.size()) + " world maps created.");
}

World::~World() = default;

World& World::getInstance() {
	static World instance; // Java: SingletonHolder
	return instance;
}

void World::storeObject(VisibleObject& object) {
	if (!object.getPosition()) {
		log.error("Tried to add {} with null position in world", object.toString());
		return;
	}
	runtime::Ptr<VisibleObject> oldObject = allObjects.putIfAbsent(object.getObjectId(), runtime::Ref<VisibleObject>(object));
	if (oldObject)
		throw exceptions::DuplicateAionObjectException(object, oldObject);

	if (auto* siegeNpc = dynamic_cast<SiegeNpc*>(&object)) {
		localSiegeNpcs.computeIfAbsent(siegeNpc->getSiegeId(), [] { return runtime::RcArrayList<runtime::Ref<SiegeNpc>>::create(); })
			->add(runtime::Ref<SiegeNpc>(*siegeNpc));
	} else if (auto* player = dynamic_cast<Player*>(&object)) {
		allPlayers->add(*player);
	}
}

bool World::removeObject(VisibleObject& object) {
	bool removed = false;
	SYNCHRONIZED(object) {
		runtime::Ptr<VisibleObject> worldObject = allObjects.get(object.getObjectId());
		if (worldObject.get() == &object) {
			try {
				if (object.isSpawned())
					despawn(object);
				object.getController().onDelete();
			} catch (const std::exception& e) {
				log.error(object.toString() + " did not leave world cleanly", e);
			}
			removed = allObjects.remove(object.getObjectId(), runtime::Ref<VisibleObject>(object));
		} else if (worldObject) {
			log.warn("Attempt to remove " + object.toString() + " from world but ID already belongs to " + worldObject->toString(),
				stackTrace("java.lang.Exception"));
		}
	}
	if (removed) {
		if (auto* siegeNpc = dynamic_cast<SiegeNpc*>(&object)) {
			localSiegeNpcs.get(siegeNpc->getSiegeId())->remove(runtime::Ref<SiegeNpc>(*siegeNpc));
		} else if (auto* player = dynamic_cast<Player*>(&object)) {
			allPlayers->remove(*player);
		}
	}
	return removed;
}

std::vector<runtime::Ptr<SiegeNpc>> World::getLocalSiegeNpcs(int32_t locationId) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<SiegeNpc>>> result = localSiegeNpcs.get(locationId);
	return result ? result->snapshot() : std::vector<runtime::Ptr<SiegeNpc>>();
}

runtime::Ptr<Player> World::getPlayer(std::string_view name) {
	return allPlayers->get(name);
}

runtime::Ptr<Player> World::getPlayer(int32_t objectId) {
	return allPlayers->get(objectId);
}

runtime::Ptr<VisibleObject> World::findVisibleObject(int32_t objectId) {
	return allObjects.get(objectId);
}

bool World::isInWorld(int32_t objectId) {
	return allObjects.containsKey(objectId);
}

runtime::Ptr<WorldMap> World::getWorldMap(int32_t id) {
	return worldMaps.get(id);
}

void World::updatePosition(VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading) {
	updatePosition(object, newX, newY, newZ, newHeading, true);
}

void World::updatePosition(VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading, bool updateKnownList) {
	if (!object.isSpawned()) { // despawned objects should never move
		log.warn("Can't update position of despawned object: {}", object.toString(), stackTrace("java.lang.Throwable"));
		return;
	}

	runtime::Ptr<WorldPosition> position = object.getPosition();
	runtime::Ptr<MapRegion> oldRegion = position->getMapRegion();
	if (!oldRegion) {
		if (auto* player = dynamic_cast<Player*>(&object)) {
			if (!player->isStaff()) {
				utils::audit::AuditLogger::log(*player, "is outside valid regions: " + position->toString());
				// he will be sent to bind point in PlayerLeaveWorldService
				auto kick = network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_KICK_CHARACTER();
				player->getClientConnection()->close(kick);
			}
		} else {
			log.warn("Old MapRegion was null when trying to update position of {}", object.toString(), stackTrace("java.lang.Throwable"));
		}
		return;
	}

	runtime::Ptr<MapRegion> newRegion = oldRegion->getParent().getRegion(newX, newY, newZ);
	if (!newRegion) {
		log.warn("New MapRegion for {} doesn't exist at coordinates: Map {}, X {}, Y {}, Z {}", object.toString(), object.getWorldId(),
			JavaFloat::toString(newX), JavaFloat::toString(newY), JavaFloat::toString(newZ), stackTrace("java.lang.Throwable"));
		if (auto* creature = dynamic_cast<Creature*>(&object))
			creature->getMoveController()->abortMove();
		if (auto* player = dynamic_cast<Player*>(&object)) {
			float x, y, z;
			int32_t worldId;
			int8_t h = 0;

			if (runtime::Ptr<model::gameobjects::player::BindPointPosition> bplist = player->getBindPoint()) {
				worldId = bplist->getMapId();
				x = bplist->getX();
				y = bplist->getY();
				z = bplist->getZ();
				h = bplist->getHeading();
			} else {
				const dataholders::PlayerInitialData::LocationData& locationData =
					dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player->getCommonData()->getRace());
				worldId = locationData.getMapId();
				x = locationData.getX();
				y = locationData.getY();
				z = locationData.getZ();
			}
			setPosition(object, worldId, x, y, z, h);
		}
		return;
	}

	position->setXYZH(newX, newY, newZ, newHeading);

	if (newRegion != oldRegion) {
		if (auto* creature = dynamic_cast<Creature*>(&object)) {
			oldRegion->revalidateZones(*creature);
			newRegion->revalidateZones(*creature);
		}
		oldRegion->remove(object);
		newRegion->add(object);
		position->setMapRegion(newRegion);
	}

	if (updateKnownList)
		object.updateKnownlist();
}

bool World::setPosition(VisibleObject& object, int32_t mapId, float x, float y, float z, int8_t heading) {
	int32_t instanceId = 1;
	if (object.getPosition() && object.getPosition()->getMapId() == mapId)
		instanceId = object.getInstanceId();
	return setPosition(runtime::Ptr<VisibleObject>(object), mapId, instanceId, x, y, z, heading);
}

bool World::setPosition(runtime::Ptr<VisibleObject> object, int32_t mapId, int32_t instance, float x, float y, float z, int8_t heading) {
	if (!object)
		return false;
	runtime::Ref<WorldPosition> pos = createPosition(mapId, x, y, z, heading, instance);
	if (!pos) // Java: pos == null (createPosition throws instead of returning null)
		return false;
	if (object->isSpawned())
		despawn(*object);
	object->setPosition(pos);
	return true;
}

runtime::Ref<WorldPosition> World::createPosition(int32_t mapId, float x, float y, float z, int8_t heading, int32_t instanceId) {
	runtime::Ptr<WorldMap> map = getWorldMap(mapId);
	if (!map)
		throw runtime::NullPointerException("Failed to create position (invalid mapId: " + std::to_string(mapId) + ")");
	if (!map->getWorldMapInstance(instanceId))
		throw runtime::NullPointerException(
			"Failed to create position (invalid instanceId " + std::to_string(instanceId) + " for mapId " + std::to_string(mapId) + ")");
	runtime::Ptr<MapRegion> mr = map->getWorldMapInstance(instanceId)->getRegion(x, y, z);
	if (!mr)
		throw runtime::NullPointerException("Failed to create position (invalid coords: x=" + JavaFloat::toString(x) + ", y=" + JavaFloat::toString(y) +
			", z=" + JavaFloat::toString(z) + " for mapId " + std::to_string(mapId) + " in instanceId " + std::to_string(instanceId) + ")");
	return WorldPosition::create(mapId, x, y, z, heading, mr);
}

void World::spawn(runtime::Ptr<VisibleObject> object) {
	if (!object)
		return;
	runtime::Ptr<WorldPosition> position = object->getPosition();
	if (position->isSpawned())
		throw exceptions::AlreadySpawnedException(*object);

	object->getController().onBeforeSpawn();
	position->setIsSpawned(true);

	position->getMapRegion()->getParent().addObject(*object);
	position->getMapRegion()->add(*object);
	object->getController().onAfterSpawn();

	object->updateKnownlist();
}

void World::despawn(VisibleObject& object) {
	despawn(object, model::animations::ObjectDeleteAnimation::FADE_OUT);
}

void World::despawn(VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	runtime::Ptr<WorldPosition> position = object.getPosition();
	std::exception_ptr failure;
	try {
		object.getController().onDespawn();
	} catch (...) {
		failure = std::current_exception();
	}
	// Java: finally
	runtime::Ptr<MapRegion> oldMapRegion = position->getMapRegion();
	position->setIsSpawned(false);
	if (oldMapRegion) { // can be null if an instance gets deleted?
		oldMapRegion->getParent().removeObject(object);
		oldMapRegion->remove(object);
		if (auto* creature = dynamic_cast<Creature*>(&object))
			oldMapRegion->revalidateZones(*creature);
	}
	object.clearKnownlist(animation);
	if (failure)
		std::rethrow_exception(failure);
}

void World::updateCachedPlayerName(std::string_view oldName, Player& player) {
	allPlayers->updateCachedPlayerName(oldName, player);
}

std::vector<runtime::Ptr<Player>> World::getAllPlayers() {
	return allPlayers->getAllPlayers();
}

void World::forEachPlayer(const std::function<void(Player&)>& consumer) {
	utils::collections::CollectionUtil::forEach(*allPlayers, [&consumer](const runtime::Ptr<Player>& player) { consumer(*player); });
}

void World::forEachObject(const std::function<void(VisibleObject&)>& consumer) {
	utils::collections::CollectionUtil::forEach(allObjects.values(), [&consumer](const runtime::Ptr<VisibleObject>& object) { consumer(*object); });
}

} // namespace aion::gameserver::world
