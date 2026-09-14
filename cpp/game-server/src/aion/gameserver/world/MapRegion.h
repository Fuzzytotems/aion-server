#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/ai/event/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world {

/**
 * Just some part of map.
 * <p>
 * Hub header (docs/design/hub-headers.md). An OwnedPart of its WorldMapInstance (cycles review; runtime-architecture.md §2.3 PartMap): Java
 * `new MapRegion(id, parent, zones)` in WorldMap2DInstance/WorldMap3DInstance::createMapRegion is `std::make_unique<MapRegion>(...)`, stored by
 * initMapRegions into WorldMapInstance.regions. `parent` is the non-retaining OwnerRef, and a Ref to a region (WorldPosition.mapRegion)
 * retains the instance. `neighboursIncludingSelf` holds sibling pointers to regions of the same instance (`{ this }` first, as in Java), so
 * neither the self-reference nor the neighbour links retain anything.
 *
 * @author -Nemesiss-
 */
class MapRegion : public runtime::OwnedPart {
private:
	/** Java: Comparator.comparing(zone type).thenComparingInt(priority).thenComparingInt(name id); callback struct in MapRegion.cpp */
	static const runtime::PinnedCallback<int32_t(zone::ZoneInstance&, zone::ZoneInstance&)> zoneComparator;
	const int32_t regionId;
	runtime::OwnerRef<WorldMapInstance> parent; // fieldmap: part owner (build/s0b-cycles-work/setII.toml, cycles.toml `part`)
	// fieldmap: sibling pointers to regions of the same instance (build/s0b-cycles-work/setII.toml, cycles.toml `part`)
	runtime::Field<runtime::Ref<runtime::Array<MapRegion*>>> neighboursIncludingSelf;
	const runtime::Ref<runtime::Array<runtime::Ref<zone::ZoneInstance>>> zonesSortedByTypeAndPriority;
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::VisibleObject>> objects{};
	runtime::Field<int32_t> playerCount{};
	runtime::Field<bool> regionActive{false};
	runtime::Field<bool> deactivationPending{false};

public:
	/** Java package-private constructor (a part of `parent`). Sorts the given zones with zoneComparator. */
	MapRegion(int32_t id, WorldMapInstance& parent, std::span<const runtime::Ptr<zone::ZoneInstance>> zones);
	~MapRegion() override;

	int32_t getRegionId() const { return regionId; }

	WorldMapInstance& getParent() const { return parent; }

	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::VisibleObject>>& getObjects() { return objects; }

	/** Java returns the live array (MapRegion[]); it is replaced, never modified, by addNeighbourRegion. Elements are regions of the same instance. */
	runtime::Ptr<runtime::Array<MapRegion*>> getNeighbours() const { return neighboursIncludingSelf.get(); }

	/** Java package-private */
	void addNeighbourRegion(MapRegion& neighbour);

	/** Java package-private */
	void add(model::gameobjects::VisibleObject& object);

	/** Java package-private */
	void remove(model::gameobjects::VisibleObject& object);

private:
	int32_t getPlayerCount(); // synchronized

	int32_t incrementPlayerCount(); // synchronized

	int32_t decrementPlayerCount(); // synchronized

	bool setRegionState(bool active); // synchronized

	void activate();

	void scheduleDeactivation();

	void tryDeactivate();

	void notifyCreatures(ai::event::AIEventType event);

public:
	bool isActive(); // synchronized

private:
	bool anyNeighbourHasPlayers();

public:
	void revalidateZones(model::gameobjects::Creature& creature);

	std::vector<runtime::Ptr<zone::ZoneInstance>> findZones(model::gameobjects::Creature& creature);

	bool onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target);

	bool isInsideZone(const zone::ZoneName* zoneName, float x, float y, float z);

	bool isInsideZone(const zone::ZoneName* zoneName, model::gameobjects::Creature& creature);

	/**
	 * Item use zones always have the same names instances, while we have unique names; Thus, a special check for item use.
	 */
	bool isInsideItemUseZone(const zone::ZoneName* zoneName, model::gameobjects::Creature& creature);

	int32_t getZoneCount();
};

} // namespace aion::gameserver::world
