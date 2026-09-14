#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/siege/fwd.h"
#include "aion/gameserver/world/container/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::world {

/**
 * World object for storing and spawning, despawning etc players and other in-game objects. It also manage WorldMaps and instances.
 * <p>
 * Hub header (docs/design/hub-headers.md). An Immortal singleton (fieldmap base Immortal, hub-headers.md §11.2). `allObjects` retains every
 * object stored in the world until removeObject (Java: the object is reachable through World).
 *
 * @author -Nemesiss-, Source, Wakizashi
 */
class World : public runtime::Immortal {
private:
	/** Container with all players that entered world. */
	const runtime::Ref<container::PlayerContainer> allPlayers;
	/** Container with all VisibleObjects in the world. */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::VisibleObject>> allObjects{AION_LOCK_CLASS(World::allObjects#stripe)};
	/** Container of SiegeNpcs by siege location id */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::gameobjects::siege::SiegeNpc>>>> localSiegeNpcs{
		AION_LOCK_CLASS(World::localSiegeNpcs#stripe)};
	/** World maps supported by server. */
	runtime::HashMap<int32_t, runtime::Ref<WorldMap>> worldMaps{AION_LOCK_CLASS(World::worldMaps)};

	/** Constructor. Creates the world maps of WORLD_MAPS_DATA. */
	World();
	~World();

public:
	static World& getInstance();

	/**
	 * Store object in the world.
	 *
	 * @throws DuplicateAionObjectException
	 *           if an object with the same id already exists
	 */
	void storeObject(model::gameobjects::VisibleObject& object);

	/**
	 * Removes this object from world. Object will be despawned first if it's spawned.
	 *
	 * @return True if the object was removed successfully, false if it was not in the world.
	 */
	bool removeObject(model::gameobjects::VisibleObject& object);

	/** Java: Collection<SiegeNpc> (the live list or an empty set): a snapshot */
	std::vector<runtime::Ptr<model::gameobjects::siege::SiegeNpc>> getLocalSiegeNpcs(int32_t locationId);

	/** Finds player by name. @return Player or null if not found */
	runtime::Ptr<model::gameobjects::player::Player> getPlayer(std::string_view name);

	/** Finds player by object id. @return Player or null if not found */
	runtime::Ptr<model::gameobjects::player::Player> getPlayer(int32_t objectId);

	/** @return VisibleObject or null if not found */
	runtime::Ptr<model::gameobjects::VisibleObject> findVisibleObject(int32_t objectId);

	/** Check whether object is in world */
	bool isInWorld(int32_t objectId);

	/** Return World Map by id. @return World map or null */
	runtime::Ptr<WorldMap> getWorldMap(int32_t id);

	/** Update position of VisibleObject [used when object is moving on one map instance] and check if active map region changed. */
	void updatePosition(model::gameobjects::VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading);

	/** Update position of VisibleObject [used when object is moving on one map instance] and check if active map region changed. */
	void updatePosition(model::gameobjects::VisibleObject& object, float newX, float newY, float newZ, int8_t newHeading, bool updateKnownList);

	/** Set position of VisibleObject without spawning [object will be invisible]. If object is spawned it will be despawned first. */
	bool setPosition(model::gameobjects::VisibleObject& object, int32_t mapId, float x, float y, float z, int8_t heading);

	/**
	 * Set position of VisibleObject without spawning [object will be invisible]. If object is spawned it will be despawned first.
	 *
	 * @return false if object is null or the position could not be created
	 */
	bool setPosition(runtime::Ptr<model::gameobjects::VisibleObject> object, int32_t mapId, int32_t instance, float x, float y, float z,
		int8_t heading);

	/**
	 * Creates and return {@link WorldPosition} object with given parameters.
	 *
	 * @throws NullPointerException
	 *           if the map, instance or map region does not exist
	 */
	runtime::Ref<WorldPosition> createPosition(int32_t mapId, float x, float y, float z, int8_t heading, int32_t instanceId);

	/**
	 * Spawn VisibleObject at current position [use setPosition]. Object will be visible by others and will see other objects. Does nothing if
	 * object is null.
	 *
	 * @throws AlreadySpawnedException
	 *           when object is already spawned.
	 */
	void spawn(runtime::Ptr<model::gameobjects::VisibleObject> object);

	/** Despawn VisibleObject, object will become invisible and object position will become invalid. All others objects will be noticed. */
	void despawn(model::gameobjects::VisibleObject& object);

	/** Despawn VisibleObject, object will become invisible and object position will become invalid. All others objects will be noticed. */
	void despawn(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation);

	void updateCachedPlayerName(std::string_view oldName, model::gameobjects::player::Player& player);

	/** Java: Collection<Player> of the PlayerContainer: a snapshot */
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> getAllPlayers();

	void forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer);

	void forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer);
};

} // namespace aion::gameserver::world
