#include "aion/gameserver/world/WorldMap3DInstance.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/RegionUtil.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::world {

WorldMap3DInstance::WorldMap3DInstance(WorldMap& parentValue, int32_t instanceIdValue, int32_t maxPlayersValue,
	const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier)
	: WorldMapInstance(parentValue, instanceIdValue, maxPlayersValue, instanceHandlerSupplier) {
}

WorldMap3DInstance::~WorldMap3DInstance() = default;

runtime::Ref<WorldMap3DInstance> WorldMap3DInstance::create(WorldMap& parentValue, int32_t instanceIdValue, int32_t maxPlayersValue,
	const std::function<runtime::Ref<instance::handlers::InstanceHandler>(WorldMapInstance&)>& instanceHandlerSupplier) {
	runtime::Ref<WorldMap3DInstance> instance = runtime::makeRef<WorldMap3DInstance>(parentValue, instanceIdValue, maxPlayersValue, instanceHandlerSupplier);
	instance->initMapRegions(); // Java: the last statement of the WorldMapInstance constructor
	return instance;
}

runtime::Ptr<MapRegion> WorldMap3DInstance::getRegion(float x, float y, float z) {
	int32_t regionId = RegionUtil::get3dRegionId(x, y, z);
	return regions.get(regionId);
}

void WorldMap3DInstance::initMapRegions() {
	int32_t size = getParent()->getWorldSize();
	// Java: float maxZ = Math.round((float) size / regionSize) * regionSize (Math.round(float) = (int) floor(x + 0.5f))
	float maxZ = static_cast<float>(static_cast<int32_t>(std::floor(static_cast<float>(size) / static_cast<float>(regionSize()) + 0.5f)) * regionSize());

	std::vector<int32_t> regionIds;
	for (int32_t x = 0; x <= size; x = x + regionSize()) {
		for (int32_t y = 0; y <= size; y = y + regionSize()) {
			for (int32_t z = 0; static_cast<float>(z) < maxZ; z = z + regionSize()) {
				regionIds.push_back(RegionUtil::get3dRegionId(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
			}
		}
	}
	// Performance (World creation, see World::World; the same regions either way): inside World creation (its explicit QuiescentOptIn under an
	// active QuiescentScope, whose frames and this frame hold no borrow) the Reclaimer must be able to free the temporaries of the zone tests while the loop runs (11 million
	// Point3D for this map). From a thread that can fork, every region is created in its own task scope (PER_ELEMENT; this thread is unpublished
	// while it waits), and an element waits before it borrows anything while the backlog exceeds half the watchdog's dump threshold (the
	// helpers retire faster than the Reclaimer thread destroys; the throttle ends after a wait of 2 s that did not help); otherwise (a ForkJoin
	// helper, serial pool) in order with a quiescent point per region.
	auto putRegion = [this](int32_t regionId) {
		std::unique_ptr<MapRegion> mapRegion = createMapRegion(regionId);
		SYNCHRONIZED(regions.monitor()) {
			regions.put(regionId, std::move(mapRegion));
		}
	};
	bool quiescent = runtime::QuiescentOptIn::active(runtime::QuiescentOptIn::WORLD_CREATION);
	if (quiescent && runtime::ForkJoinPool::commonPool().isParallelFromCurrentThread()) {
		const uint64_t backlogLimit = std::max<uint64_t>(runtime::Reclaimer::getInstance().getConfig().backlogDumpObjects / 2, 1);
		runtime::AtomicBoolean throttle{true}; // off after the first wait that timed out (something else pins the backlog)
		runtime::quiescentPoint(); // quiescent-safe: this instance under construction is kept by create()'s Ref; no borrow in this frame
		runtime::ForkJoinPool::commonPool().parallelForEach(
			regionIds,
			[&putRegion, &throttle, backlogLimit](int32_t regionId) {
				runtime::Reclaimer& reclaimer = runtime::Reclaimer::getInstance();
				if (throttle.get() && !reclaimer.awaitBacklogBelow(backlogLimit, std::chrono::seconds(2)))
					throttle.set(false);
				putRegion(regionId);
			},
			runtime::Isolation::PER_ELEMENT);
	} else if (quiescent) {
		for (int32_t regionId : regionIds) {
			runtime::quiescentPoint(); // quiescent-safe: as above (region ids only)
			putRegion(regionId);
		}
	} else {
		// Java: regionIds.parallelStream().forEach(...), helpers join the caller's scope (this instance is not published yet)
		runtime::ForkJoinPool::commonPool().parallelForEach(regionIds, [&putRegion](int32_t regionId) { putRegion(regionId); });
	}

	// Add Neighbour
	for (int32_t x = 0; x <= size; x = x + regionSize()) {
		if (quiescent)
			runtime::quiescentPoint(); // quiescent-safe: no borrow is held between rows (the replaced neighbour arrays are retired here)
		for (int32_t y = 0; y <= size; y = y + regionSize()) {
			for (int32_t z = 0; static_cast<float>(z) < maxZ; z = z + regionSize()) {
				int32_t regionId = RegionUtil::get3dRegionId(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
				runtime::Ptr<MapRegion> mapRegion = regions.get(regionId);
				for (int32_t x2 = x - regionSize(); x2 <= x + regionSize(); x2 += regionSize()) {
					for (int32_t y2 = y - regionSize(); y2 <= y + regionSize(); y2 += regionSize()) {
						for (int32_t z2 = z - regionSize(); z2 < z + regionSize(); z2 += regionSize()) {
							if (x2 == x && y2 == y && z2 == z)
								continue;
							int32_t neighbourId = RegionUtil::get3dRegionId(static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2));
							runtime::Ptr<MapRegion> neighbour = regions.get(neighbourId);
							if (neighbour)
								mapRegion->addNeighbourRegion(*neighbour);
						}
					}
				}
			}
		}
	}
}

std::unique_ptr<MapRegion> WorldMap3DInstance::createMapRegion(int32_t regionId) {
	float startX = static_cast<float>(RegionUtil::getXFrom3dRegionId(regionId));
	float startY = static_cast<float>(RegionUtil::getYFrom3dRegionId(regionId));
	float startZ = static_cast<float>(RegionUtil::getZFrom3dRegionId(regionId));
	std::vector<runtime::Ptr<zone::ZoneInstance>> regionZones =
		filterZones(getMapId(), regionId, startX, startY, startZ, startZ + static_cast<float>(regionSize()));
	return std::make_unique<MapRegion>(regionId, *this, regionZones);
}

bool WorldMap3DInstance::isPersonal() {
	return false;
}

int32_t WorldMap3DInstance::getOwnerId() {
	return 0;
}

} // namespace aion::gameserver::world
