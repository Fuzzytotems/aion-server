#include "aion/gameserver/services/rift/RiftOpenRunnable.h"

#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/rift/RiftInformer.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services::rift {

// Java RiftOpenRunnable.java:20-27. The close lambda (RiftOpenRunnable.java:23) captures the world id by value; the service is immortal.
void RiftOpenRunnable::operator()() const {
	RiftService::getInstance().prepareRiftOpening(worldId, guards);
	// Scheduled rifts close
	const int32_t world = worldId;
	utils::ThreadPoolManager::getInstance().schedule({&RiftService::getInstance()},
		[world] { RiftService::getInstance().closeAutoCloseableRifts(world); }, RiftService::getInstance().getDuration() * 3540 * 1000);
	// Broadcast rift spawn on map
	RiftInformer::sendRiftsInfo(worldId);
}

} // namespace aion::gameserver::services::rift
