#include "aion/gameserver/services/RiftService.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/schedule/RiftSchedule.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RiftData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

void RiftService::initRiftLocations() {
	// C++: Java assigns the data holder's map (or Collections.emptyMap()); the port publishes a copy (hub-headers.md §11.1 empty collections)
	runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>>> riftLocations =
		runtime::RcHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>>::create(AION_LOCK_CLASS(RiftService::locations));
	if (configs::main::CustomConfig::RIFT_ENABLED.load()) {
		for (const auto& [id, location] : dataholders::DataManager::RIFT_DATA->getRiftLocations())
			riftLocations->put(id, location);
	}
	locations.set(std::move(riftLocations));
}

void RiftService::initRifts() {
	if (configs::main::CustomConfig::RIFT_ENABLED.load()) {
		// M5a keeps rifts disabled (plan D1): RiftSchedule.load() and the RiftOpenRunnable cron jobs follow with the rift work
		AION_UNPORTED();
	}
}

bool RiftService::isValidId(int32_t id) {
	if (isRift(id)) {
		return RiftService::getInstance().getRiftLocations()->containsKey(id);
	} else {
		for (const runtime::Ptr<model::rift::RiftLocation>& loc : RiftService::getInstance().getRiftLocations()->values()) {
			if (loc->getWorldId() == id)
				return true;
		}
	}
	return false;
}

bool RiftService::isRift(int32_t id) {
	return id < 10000;
}

bool RiftService::openRifts(int32_t id, bool guards) {
	AION_UNPORTED();
}

bool RiftService::closeRifts(int32_t id) {
	AION_UNPORTED();
}

void RiftService::prepareRiftOpening(int32_t id, bool guards) {
	AION_UNPORTED();
}

void RiftService::openRifts(model::rift::RiftLocation& location, bool isWithGuards) {
	AION_UNPORTED();
}

void RiftService::closeRift(model::rift::RiftLocation& location) {
	AION_UNPORTED();
}

bool RiftService::isRiftOpened(int32_t riftId) {
	return activeRifts.containsKey(riftId);
}

void RiftService::closeAutoCloseableRifts(int32_t worldId) {
	AION_UNPORTED();
}

void RiftService::updateSpawned(int32_t oldObjectId, model::gameobjects::VisibleObject& respawn) {
	for (const runtime::Ptr<model::rift::RiftLocation>& loc : locations.get()->values()) {
		if (loc->replaceSpawned(oldObjectId, respawn))
			break;
	}
}

int32_t RiftService::getDuration() {
	return configs::main::CustomConfig::RIFT_DURATION.load();
}

runtime::Ptr<model::rift::RiftLocation> RiftService::getRiftLocation(int32_t id) {
	return locations.get()->get(id);
}

RiftService& RiftService::getInstance() {
	static RiftService instance; // Java RiftServiceHolder
	return instance;
}

} // namespace aion::gameserver::services
