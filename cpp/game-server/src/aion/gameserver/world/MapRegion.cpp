#include "aion/gameserver/world/MapRegion.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::world {

namespace {

using model::templates::zone::ZoneClassName;

/**
 * Java: `Comparator.comparing((ZoneInstance z) -> z.getZoneTemplate().getZoneType()).thenComparingInt(z -> z.getZoneTemplate().getPriority())
 * .thenComparingInt(z -> z.getZoneTemplate().getName().id())` (MapRegion.java:23): the enum ordinal, then Integer.compare twice.
 */
struct ZoneComparator : runtime::TaskStruct {
	int32_t operator()(zone::ZoneInstance& first, zone::ZoneInstance& second) const {
		const auto* a = first.getZoneTemplate();
		const auto* b = second.getZoneTemplate();
		if (a->getZoneType() != b->getZoneType())
			return a->getZoneType() < b->getZoneType() ? -1 : 1;
		if (a->getPriority() != b->getPriority())
			return a->getPriority() < b->getPriority() ? -1 : 1;
		int32_t aId = a->getName()->id();
		int32_t bId = b->getName()->id();
		return aId == bId ? 0 : (aId < bId ? -1 : 1);
	}
};

/** Java `new ZoneInstance[] {...}` for the zones passed to the constructor (the array is sorted in place afterwards). */
runtime::Ref<runtime::Array<runtime::Ref<zone::ZoneInstance>>> toZoneArray(std::span<const runtime::Ptr<zone::ZoneInstance>> zones) {
	auto array = runtime::Array<runtime::Ref<zone::ZoneInstance>>::make(static_cast<int32_t>(zones.size()));
	for (size_t i = 0; i < zones.size(); ++i)
		(*array)[static_cast<int32_t>(i)].set(zones[i]);
	return array;
}

/** Java: object instanceof Player */
bool isPlayer(model::gameobjects::VisibleObject& object) {
	return dynamic_cast<model::gameobjects::player::Player*>(&object) != nullptr;
}

} // namespace

const runtime::PinnedCallback<int32_t(zone::ZoneInstance&, zone::ZoneInstance&)> MapRegion::zoneComparator =
	runtime::PinnedCallback<int32_t(zone::ZoneInstance&, zone::ZoneInstance&)>(ZoneComparator{});

MapRegion::MapRegion(int32_t id, WorldMapInstance& parentValue, std::span<const runtime::Ptr<zone::ZoneInstance>> zones)
	: OwnedPart(parentValue), regionId(id), parent(parentValue), neighboursIncludingSelf(runtime::Array<MapRegion*>::of({this})),
	  zonesSortedByTypeAndPriority(toZoneArray(zones)) {
	// Java: Arrays.sort(zonesSortedByTypeAndPriority, zoneComparator) - a stable merge sort (TimSort) of the array this region owns
	std::vector<runtime::Ptr<zone::ZoneInstance>> sorted(zones.begin(), zones.end());
	std::stable_sort(sorted.begin(), sorted.end(),
		[](const runtime::Ptr<zone::ZoneInstance>& a, const runtime::Ptr<zone::ZoneInstance>& b) { return zoneComparator(*a, *b) < 0; });
	for (size_t i = 0; i < sorted.size(); ++i)
		(*zonesSortedByTypeAndPriority)[static_cast<int32_t>(i)].set(sorted[i]);
}

MapRegion::~MapRegion() = default;

void MapRegion::addNeighbourRegion(MapRegion& neighbour) {
	// Java: neighboursIncludingSelf = Arrays.copyOf(neighboursIncludingSelf, length + 1); last = neighbour (a new array, never modified afterwards)
	runtime::Ptr<runtime::Array<MapRegion*>> current = neighboursIncludingSelf.get();
	auto copy = runtime::Array<MapRegion*>::make(current->length() + 1);
	for (int32_t i = 0; i < current->length(); ++i)
		(*copy)[i].set(current->get(i));
	(*copy)[current->length()].set(&neighbour);
	neighboursIncludingSelf.set(copy);
}

void MapRegion::add(model::gameobjects::VisibleObject& object) {
	if (!objects.put(object.getObjectId(), runtime::Ref<model::gameobjects::VisibleObject>(object)) && isPlayer(object) && incrementPlayerCount() == 1)
		activate();
}

void MapRegion::remove(model::gameobjects::VisibleObject& object) {
	runtime::Ptr<model::gameobjects::VisibleObject> removed = objects.remove(object.getObjectId());
	if (removed && isPlayer(*removed) && decrementPlayerCount() == 0)
		scheduleDeactivation();
}

int32_t MapRegion::getPlayerCount() {
	SYNCHRONIZED(*this) {
		return playerCount.get();
	}
}

int32_t MapRegion::incrementPlayerCount() {
	SYNCHRONIZED(*this) {
		int32_t count = playerCount.get() + 1;
		playerCount.set(count);
		return count;
	}
}

int32_t MapRegion::decrementPlayerCount() {
	SYNCHRONIZED(*this) {
		if (playerCount.get() == 0)
			return 0;
		int32_t count = playerCount.get() - 1;
		playerCount.set(count);
		return count;
	}
}

bool MapRegion::setRegionState(bool active) {
	SYNCHRONIZED(*this) {
		if (regionActive.get() == active)
			return false;
		regionActive.set(active);
		return true;
	}
}

void MapRegion::activate() {
	auto activatedRegions = runtime::RcArrayList<runtime::Ref<MapRegion>>::create();
	for (MapRegion* mapRegion : *neighboursIncludingSelf.get()) {
		if (mapRegion->setRegionState(true))
			activatedRegions->add(runtime::Ref<MapRegion>(*mapRegion));
	}
	if (!activatedRegions->isEmpty()) {
		// task lambda MapRegion@L100:44: pin {}, captures activatedRegions (fieldmap: const Ref<RcArrayList<Ref<MapRegion>>>)
		utils::ThreadPoolManager::getInstance().execute(runtime::Pin(), [activatedRegions] {
			for (runtime::Ptr<MapRegion> region : *activatedRegions)
				region->notifyCreatures(ai::event::AIEventType::ACTIVATE);
		});
	}
}

void MapRegion::scheduleDeactivation() {
	if (deactivationPending.get())
		return;
	deactivationPending.set(true);
	// task lambda MapRegion@L107:44: pin {this}
	utils::ThreadPoolManager::getInstance().schedule({this}, [this] {
		deactivationPending.set(false);
		if (getPlayerCount() == 0) {
			for (MapRegion* mapRegion : *neighboursIncludingSelf.get())
				mapRegion->tryDeactivate();
		}
	}, 60, runtime::TimeUnit::SECONDS);
}

void MapRegion::tryDeactivate() {
	if (getParent().getParent()->isInstanceType() || getParent().getMapId() == getId(WorldMapType::TRANSIDIUM_ANNEX))
		return;
	if (!isActive() || anyNeighbourHasPlayers())
		return;
	if (!setRegionState(false))
		return;
	notifyCreatures(ai::event::AIEventType::DEACTIVATE);
}

void MapRegion::notifyCreatures(ai::event::AIEventType event) {
	for (runtime::Ptr<model::gameobjects::VisibleObject> visObject : objects.values()) {
		if (auto creature = runtime::as<model::gameobjects::Creature>(visObject))
			creature->getAi().onGeneralEvent(event);
	}
}

bool MapRegion::isActive() {
	SYNCHRONIZED(*this) {
		return regionActive.get();
	}
}

bool MapRegion::anyNeighbourHasPlayers() {
	for (MapRegion* r : *neighboursIncludingSelf.get()) {
		if (r->getPlayerCount() > 0)
			return true;
	}
	return false;
}

void MapRegion::revalidateZones(model::gameobjects::Creature& creature) {
	std::optional<ZoneClassName> zoneType; // Java: ZoneClassName zoneType = null
	bool enteredPriorityZone = false;
	for (runtime::Ptr<zone::ZoneInstance> zone : *zonesSortedByTypeAndPriority) {
		if (zoneType != zone->getZoneTemplate()->getZoneType()) {
			zoneType = zone->getZoneTemplate()->getZoneType();
			enteredPriorityZone = false;
		}
		if (!creature.isSpawned() || enteredPriorityZone || !zone->revalidate(creature)) {
			zone->onLeave(creature);
			continue;
		}
		if (zone->getZoneTemplate()->getPriority() != 0) {
			enteredPriorityZone = true;
		}
		zone->onEnter(creature);
	}
}

std::vector<runtime::Ptr<zone::ZoneInstance>> MapRegion::findZones(model::gameobjects::Creature& creature) {
	std::vector<runtime::Ptr<zone::ZoneInstance>> z;
	for (runtime::Ptr<zone::ZoneInstance> zone : *zonesSortedByTypeAndPriority) {
		if (zone->isInsideCreature(creature)) {
			z.push_back(zone);
		}
	}
	return z;
}

bool MapRegion::onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target) {
	for (runtime::Ptr<zone::ZoneInstance> zone : *zonesSortedByTypeAndPriority) {
		if (zone->isInsideCreature(target)) {
			if (zone->onDie(attacker, target))
				return true;
		}
	}
	return false;
}

bool MapRegion::isInsideZone(const zone::ZoneName* zoneName, float x, float y, float z) {
	for (runtime::Ptr<zone::ZoneInstance> zone : *zonesSortedByTypeAndPriority) {
		if (zone->getZoneTemplate()->getName() != zoneName)
			continue;
		return zone->isInsideCordinate(x, y, z);
	}
	return false;
}

bool MapRegion::isInsideZone(const zone::ZoneName* zoneName, model::gameobjects::Creature& creature) {
	for (runtime::Ptr<zone::ZoneInstance> zone : *zonesSortedByTypeAndPriority) {
		if (zone->getZoneTemplate()->getName() == zoneName)
			return zone->isInsideCreature(creature);
	}
	return false;
}

bool MapRegion::isInsideItemUseZone(const zone::ZoneName* zoneName, model::gameobjects::Creature& creature) {
	bool checkFortresses = zoneName->name() == "_ABYSS_CASTLE_AREA_"; // some items have this special zonename in uselimits
	for (runtime::Ptr<zone::ZoneInstance> zone : *zonesSortedByTypeAndPriority) {
		if (checkFortresses) {
			if (zone->getZoneTemplate()->getZoneType() != ZoneClassName::FORT)
				continue;
		} else if (!zone->getZoneTemplate()->getXmlName().starts_with(zoneName->name())) { // Java: zoneName.toString()
			continue;
		}
		if (zone->isInsideCreature(creature))
			return true;
	}
	return false;
}

int32_t MapRegion::getZoneCount() {
	return zonesSortedByTypeAndPriority->length();
}

} // namespace aion::gameserver::world
