#pragma once

#include <cstdint>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/world/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world {

/**
 * This object is representing one in-game map and can have instances.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4): held by `World::worldMaps` and by every instance
 * (`WorldMapInstance::parent`). Java `Iterable<WorldMapInstance>`: iterator()/begin()/end() iterate a snapshot of the instances (§7.2).
 * The constructor creates the instances through WorldMapInstanceFactory, so it stays unported.
 *
 * @author -Nemesiss-
 */
class WorldMap : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const model::templates::world::WorldMapTemplate* worldMapTemplate;
	runtime::AtomicInteger nextInstanceId{AION_LOCK_CLASS(WorldMap::nextInstanceId)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<WorldMapInstance>> instances{AION_LOCK_CLASS(WorldMap::instances#stripe)};
	runtime::Field<int32_t> worldOptions{};

protected:
	explicit WorldMap(const model::templates::world::WorldMapTemplate* worldMapTemplate);
	~WorldMap() override;

public:
	/** Java: new WorldMap(worldMapTemplate) */
	static runtime::Ref<WorldMap> create(const model::templates::world::WorldMapTemplate* worldMapTemplate);

	std::string getName();

	int32_t getWaterLevel();

	int32_t getDeathLevel();

	WorldType getWorldType();

	int32_t getWorldSize();

	WorldDropType getWorldDropType();

	int32_t getMapId();

	bool isFlightAllowed();

	bool isExceptBuff();

	bool canGlide();

	bool canPutKisk();

	bool canRecall();

	bool canRide();

	bool canFlyRide();

	bool isPvpAllowed();

	bool isSameRaceDuelsAllowed();

	bool isOtherRaceDuelsAllowed();

	bool canReturnToBattle();

	void setWorldOption(zone::ZoneAttributes option);

	void removeWorldOption(zone::ZoneAttributes option);

	bool hasOverridenOption(zone::ZoneAttributes option);

	int32_t getInstanceCount();

	/**
	 * Return a WorldMapInstance - depends on map configuration one map may have twins instances to balance player. This method will return
	 * WorldMapInstance by server chose.
	 *
	 * @return WorldMapInstance.
	 */
	runtime::Ptr<WorldMapInstance> getMainWorldMapInstance();

	runtime::Ptr<WorldMapInstance> getWorldMapInstance(int32_t instanceId);

	/**
	 * Remove WorldMapInstance by instanceId.
	 */
	void removeWorldMapInstance(int32_t instanceId);

	/**
	 * Add instance to map
	 */
	void addInstance(int32_t instanceId, WorldMapInstance& instance);

	const model::templates::world::WorldMapTemplate* getTemplate() const { return worldMapTemplate; }

	/**
	 * @return the nextInstanceId
	 */
	int32_t getNextInstanceId();

	/**
	 * Whether this world map is instance type
	 */
	bool isInstanceType();

	/** Java: Iterator<WorldMapInstance> iterator() over instances.values() (§7.2) */
	runtime::JavaIterator<runtime::Ptr<WorldMapInstance>> iterator();

	/** C++ only: range-for over a snapshot of the instances (§7.2) */
	runtime::SnapshotIterator<runtime::Ptr<WorldMapInstance>> begin();

	std::default_sentinel_t end() const noexcept { return {}; }

	/**
	 * All instance ids of this map (Java: the live key set; a snapshot)
	 */
	std::vector<int32_t> getAvailableInstanceIds();

	void forEachObject(const std::function<void(model::gameobjects::VisibleObject&)>& consumer);
};

} // namespace aion::gameserver::world
