#include "aion/gameserver/world/MapRegion.h"

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::world {

namespace {

/**
 * Java: `Comparator.comparing((ZoneInstance z) -> z.getZoneTemplate().getZoneType()).thenComparingInt(z -> z.getZoneTemplate().getPriority())
 * .thenComparingInt(z -> z.getZoneTemplate().getName().id())` (MapRegion.java:23). Constructing it runs nothing; comparing is unported.
 */
struct ZoneComparator : runtime::TaskStruct {
	int32_t operator()(zone::ZoneInstance& first, zone::ZoneInstance& second) const { AION_UNPORTED(); }
};

/** Java `new ZoneInstance[] {...}` for the zones passed to the constructor (the array is sorted in place afterwards). */
runtime::Ref<runtime::Array<runtime::Ref<zone::ZoneInstance>>> toZoneArray(std::span<const runtime::Ptr<zone::ZoneInstance>> zones) {
	auto array = runtime::Array<runtime::Ref<zone::ZoneInstance>>::make(static_cast<int32_t>(zones.size()));
	for (size_t i = 0; i < zones.size(); ++i)
		(*array)[static_cast<int32_t>(i)].set(zones[i]);
	return array;
}

} // namespace

const runtime::PinnedCallback<int32_t(zone::ZoneInstance&, zone::ZoneInstance&)> MapRegion::zoneComparator =
	runtime::PinnedCallback<int32_t(zone::ZoneInstance&, zone::ZoneInstance&)>(ZoneComparator{});

MapRegion::MapRegion(int32_t id, WorldMapInstance& parentValue, std::span<const runtime::Ptr<zone::ZoneInstance>> zones)
	: OwnedPart(parentValue), regionId(id), parent(parentValue), neighboursIncludingSelf(runtime::Array<MapRegion*>::of({this})),
	  zonesSortedByTypeAndPriority(toZoneArray(zones)) {
	// Java: Arrays.sort(zonesSortedByTypeAndPriority, zoneComparator);
	AION_UNPORTED();
}

MapRegion::~MapRegion() = default;

void MapRegion::addNeighbourRegion(MapRegion& neighbour) {
	AION_UNPORTED();
}

void MapRegion::add(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void MapRegion::remove(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

int32_t MapRegion::getPlayerCount() {
	SYNCHRONIZED(*this) {
		return playerCount.get();
	}
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
int32_t MapRegion::incrementPlayerCount() {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
int32_t MapRegion::decrementPlayerCount() {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
bool MapRegion::setRegionState(bool active) {
	AION_UNPORTED();
}

void MapRegion::activate() {
	// task lambda MapRegion@L100:44 (fieldmap.py --class key com.aionemu.gameserver.world.MapRegion@L100:44)
	AION_UNPORTED();
}

void MapRegion::scheduleDeactivation() {
	// task lambda MapRegion@L107:44 (fieldmap.py --class key com.aionemu.gameserver.world.MapRegion@L107:44)
	AION_UNPORTED();
}

void MapRegion::tryDeactivate() {
	AION_UNPORTED();
}

void MapRegion::notifyCreatures(ai::event::AIEventType event) {
	AION_UNPORTED();
}

bool MapRegion::isActive() {
	SYNCHRONIZED(*this) {
		return regionActive.get();
	}
}

bool MapRegion::anyNeighbourHasPlayers() {
	AION_UNPORTED();
}

void MapRegion::revalidateZones(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<zone::ZoneInstance>> MapRegion::findZones(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool MapRegion::onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

bool MapRegion::isInsideZone(const zone::ZoneName* zoneName, float x, float y, float z) {
	AION_UNPORTED();
}

bool MapRegion::isInsideZone(const zone::ZoneName* zoneName, model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool MapRegion::isInsideItemUseZone(const zone::ZoneName* zoneName, model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

int32_t MapRegion::getZoneCount() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world
