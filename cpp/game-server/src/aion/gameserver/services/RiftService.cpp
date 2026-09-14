#include "aion/gameserver/services/RiftService.h"

#include "aion/gameserver/configs/schedule/RiftSchedule.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

void RiftService::initRiftLocations() {
	AION_UNPORTED();
}

void RiftService::initRifts() {
	AION_UNPORTED();
}

bool RiftService::isValidId(int32_t id) {
	AION_UNPORTED();
}

bool RiftService::isRift(int32_t id) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

void RiftService::closeAutoCloseableRifts(int32_t worldId) {
	AION_UNPORTED();
}

void RiftService::updateSpawned(int32_t oldObjectId, model::gameobjects::VisibleObject& respawn) {
	AION_UNPORTED();
}

int32_t RiftService::getDuration() {
	AION_UNPORTED();
}

runtime::Ptr<model::rift::RiftLocation> RiftService::getRiftLocation(int32_t id) {
	AION_UNPORTED();
}

RiftService& RiftService::getInstance() {
	static RiftService instance; // Java RiftServiceHolder
	return instance;
}

} // namespace aion::gameserver::services
