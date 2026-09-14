#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/templates/world/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world {

/**
 * World map instance object.
 * <p>
 * Hub header (docs/design/hub-headers.md). Abstract RefCounted (fieldmap K4); WorldMap2DInstance and WorldMap3DInstance provide `create`.
 * `instanceHandler` is a `Field` (fieldmap.toml: C++-only mutability, the handler is detached to a no-op handler in destroyInstance, so the
 * WorldMapInstance -> InstanceHandler -> WorldMapInstance cycle is cut). The map regions are parts of their instance (PartMap, created by
 * createMapRegion in initMapRegions): a Ref to a MapRegion (WorldPosition.mapRegion) retains the instance. The C++-only
 * detachInstanceHandler() and releaseRegisteredTeam() are the cycle breakers InstanceService.destroyInstance calls together with
 * setStartPos(nullptr) (cycles.toml). Two-phase construction: Java's constructor calls the abstract initMapRegions(), which a C++ base
 * constructor cannot dispatch to the subclass; the constructor stores the members and creates the instance handler, and the `create` of
 * WorldMap2DInstance/WorldMap3DInstance calls initMapRegions() right after constructing the object (like VisibleObject::postConstruct, before
 * the instance is published). Java `implements Iterable<VisibleObject>`: iterator() plus
 * begin()/end() for range-for over a snapshot (hub-headers.md §7.2).
 * <p>
 * Java `public static final int regionSize = WorldConfig.WORLD_REGION_SIZE` is read at class initialization, after the configs are loaded;
 * C++ static initialization runs before that, so it is the function regionSize(), which reads the config once on first use.
 *
 * @author -Nemesiss-
 */
class WorldMapInstance : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** Java: public static final int regionSize = WorldConfig.WORLD_REGION_SIZE (read once, on first use) */
	static int32_t regionSize();

private:
	const runtime::Ref<WorldMap> parent;

protected:
	runtime::PartMap<int32_t, MapRegion> regions{*this};

private:
	/** All objects spawned in this world map instance */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::VisibleObject>> worldMapObjects{
		AION_LOCK_CLASS(WorldMapInstance::worldMapObjects#stripe)};
	/** All npcs spawned in this world map instance */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::Npc>> worldMapNpcs{AION_LOCK_CLASS(WorldMapInstance::worldMapNpcs#stripe)};
	/** All players spawned in this world map instance */
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>> worldMapPlayers{
		AION_LOCK_CLASS(WorldMapInstance::worldMapPlayers#stripe)};
	runtime::ConcurrentKeySet<int32_t> registeredObjects{AION_LOCK_CLASS(WorldMapInstance::registeredObjects#stripe)};
	runtime::ConcurrentKeySet<int32_t> questIds{AION_LOCK_CLASS(WorldMapInstance::questIds#stripe)};
	// fieldmap: ZoneName is an interned immortal (fieldmap.toml [immortal]), not RefCounted
	runtime::HashMap<const zone::ZoneName*, runtime::Ref<zone::ZoneInstance>> zones{AION_LOCK_CLASS(WorldMapInstance::zones)};
	/** fieldmap.toml: C++-only mutability: the handler is detached in destroyInstance (design §3.2.2, §5.1) */
	runtime::Field<runtime::Ref<instance::handlers::InstanceHandler>> instanceHandler{};
	/** Id of this instance (channel) */
	const int32_t instanceId;
	const int32_t maxPlayers;
	runtime::Field<runtime::Ref<WorldPosition>> startPos{};
	runtime::Field<int64_t> lastPlayerLeaveTime{};
	runtime::Field<runtime::Ref<model::team::GeneralTeam>> registeredTeam{};
	runtime::Field<runtime::FutureRef> emptyInstanceTask{};
	runtime::Field<runtime::FutureRef> updateNearbyQuestsTask{};

protected:
	/**
	 * @param instanceHandlerSupplier
	 *          called once during construction with this instance (Java Function<WorldMapInstance, InstanceHandler>); returns the new handler
	 *          (InstanceEngine::getNewInstanceHandler, HandlerRegistry.h InstanceFactory)
	 */
	WorldMapInstance(WorldMap& parent, int32_t instanceId, int32_t maxPlayers,
		const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier);
	~WorldMapInstance() override;

public:
	/** Return World map id. */
	int32_t getMapId();

	/** Returns WorldMap witch is parent of this instance */
	runtime::Ptr<WorldMap> getParent() const { return parent; }

	const model::templates::world::WorldMapTemplate* getTemplate();

	/** Returns MapRegion that contains coordinates of VisibleObject. If the region doesn't exist, null is returned. (Java package-private) */
	runtime::Ptr<MapRegion> getRegion(model::gameobjects::VisibleObject& object);

	/** Returns MapRegion that contains given x,y coordinates. If the region doesn't exist, null is returned. */
	virtual runtime::Ptr<MapRegion> getRegion(float x, float y, float z) = 0;

protected:
	/** create new MapRegion and add link to neighbours. @return a new part of this instance (the caller stores it into regions) */
	virtual std::unique_ptr<MapRegion> createMapRegion(int32_t regionId) = 0;

	virtual void initMapRegions() = 0;

public:
	virtual bool isPersonal() = 0;

	virtual int32_t getOwnerId() = 0;

	/** Add VisibleObject to this world instance. */
	void addObject(model::gameobjects::VisibleObject& object);

	/** Remove VisibleObject from this world instance. */
	void removeObject(model::gameobjects::AionObject& object);

	/** @return the number of players spawned in this instance */
	int32_t getPlayerCount();

	std::vector<runtime::Ptr<model::gameobjects::player::Player>> getPlayersInside();

	runtime::Ptr<model::gameobjects::player::Player> getPlayer(int32_t objId);

	runtime::Ptr<model::gameobjects::VisibleObject> getObject(int32_t objId);

	runtime::Ptr<model::gameobjects::Npc> getNpc(int32_t npcId);

	std::vector<runtime::Ptr<model::gameobjects::Npc>> getNpcs(std::initializer_list<int32_t> npcIds);

	std::vector<runtime::Ptr<model::gameobjects::Npc>> getNpcs();

	runtime::Ptr<model::gameobjects::VisibleObject> getObjectByStaticId(int32_t staticId);

	/** @return instance id */
	int32_t getInstanceId() const { return instanceId; }

	/** Beginner instances are instances with id > twinCount. Java final. */
	bool isBeginnerInstance();

	void registerTeam(model::team::GeneralTeam& team);

	void register_(int32_t objectId);

	runtime::ConcurrentKeySet<int32_t>& getRegisteredObjects() { return registeredObjects; }

	int32_t getRegisteredCount();

	/** @return true if the object is registered in this instance */
	bool isRegistered(int32_t objectId);

	runtime::Ptr<runtime::Future> getEmptyInstanceTask() const { return emptyInstanceTask.get(); }

	/** Out of line: it releases the previous task reference. */
	void setEmptyInstanceTask(runtime::Ptr<runtime::Future> emptyInstanceTask);

	runtime::Ptr<model::team::GeneralTeam> getRegisteredTeam() const { return registeredTeam.get(); }

	runtime::ConcurrentKeySet<int32_t>& getQuestIds() { return questIds; }

	/** Java final. Never null (C++: a detached instance has a no-op handler). */
	runtime::Ptr<instance::handlers::InstanceHandler> getInstanceHandler() const { return instanceHandler.get(); }

	/**
	 * C++ only (cycle breaker, cycles.toml `WorldMapInstance.instanceHandler`): replaces the handler with a no-op handler, so the destroyed
	 * instance and its handler no longer retain each other. Called by InstanceService.destroyInstance after onInstanceDestroy().
	 */
	void detachInstanceHandler();

	/** C++ only (cycle breaker, called by InstanceService.destroyInstance): clears registeredTeam without notifications. Idempotent. */
	void releaseRegisteredTeam() noexcept;

	/** Out of line: it releases the previous position. */
	void setStartPos(runtime::Ptr<WorldPosition> startPos);

	runtime::Ptr<WorldPosition> getStartPos() const { return startPos.get(); }

protected:
	/** Java: ZoneInstance[] (the array handed to MapRegion::create) */
	std::vector<runtime::Ptr<zone::ZoneInstance>> filterZones(int32_t mapId, int32_t regionId, float startX, float startY, float minZ, float maxZ);

public:
	bool isInsideZone(model::gameobjects::VisibleObject& object, const zone::ZoneName* zoneName);

	bool isInsideZone(WorldPosition& pos, const zone::ZoneName* zoneName);

	int32_t getMaxPlayers() const { return maxPlayers; }

	bool isFull();

	int64_t getLastPlayerLeaveTime() const { return lastPlayerLeaveTime.get(); }

	void setDoorState(int32_t staticId, bool open);

	/** Java Iterable<VisibleObject>.iterator() (the live values view of worldMapObjects): iterates a snapshot, remove() removes from the map */
	runtime::JavaIterator<runtime::Ptr<model::gameobjects::VisibleObject>> iterator();

	/** C++ only: range-for over a snapshot of the spawned objects (`for (Ptr<VisibleObject> object : *instance)`) */
	runtime::SnapshotIterator<runtime::Ptr<model::gameobjects::VisibleObject>> begin();

	std::default_sentinel_t end() const noexcept { return {}; }

	void forEachNpc(const std::function<void(model::gameobjects::Npc&)>& consumer);

	void forEachPlayer(const std::function<void(model::gameobjects::player::Player&)>& consumer);

	void forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer);

	void forEachDoor(const std::function<void(model::gameobjects::StaticDoor&)>& consumer);

	std::string toString();
};

} // namespace aion::gameserver::world
