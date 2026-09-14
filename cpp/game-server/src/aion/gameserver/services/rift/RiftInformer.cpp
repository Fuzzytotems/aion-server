#include "aion/gameserver/services/rift/RiftInformer.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::services::rift {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous Consumer at RiftInformer.java:93 (com.aionemu.gameserver.services.rift.RiftInformer$1); argument 1 of forEachPlayer(); storage: sync

void RiftInformer::sendRiftsInfo(int32_t worldId) {
	AION_UNPORTED();
}

void RiftInformer::sendRiftsInfo(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void RiftInformer::sendRiftInfo(std::span<const int32_t> worlds) {
	AION_UNPORTED();
}

void RiftInformer::sendRiftDespawn(int32_t worldId, int32_t objId) {
	AION_UNPORTED();
}

std::vector<std::unique_ptr<network::aion::AionServerPacket>> RiftInformer::getPackets(int32_t worldId) {
	AION_UNPORTED();
}

std::vector<std::unique_ptr<network::aion::AionServerPacket>> RiftInformer::getPackets(int32_t worldId, int32_t objId) {
	AION_UNPORTED();
}

void RiftInformer::syncRiftsState(model::gameobjects::player::Player& player,
	const std::vector<std::unique_ptr<network::aion::AionServerPacket>>& packets) {
	AION_UNPORTED();
}

void RiftInformer::syncRiftsState(int32_t worldId, const std::vector<std::unique_ptr<network::aion::AionServerPacket>>& packets) {
	AION_UNPORTED();
}

void RiftInformer::syncRiftsState(int32_t worldId, const std::vector<std::unique_ptr<network::aion::AionServerPacket>>& packets, bool isDespawnInfo) {
	AION_UNPORTED();
}

std::map<int32_t, int32_t> RiftInformer::getAnnounceData(int32_t worldId) {
	AION_UNPORTED();
}

std::map<int32_t, int32_t> RiftInformer::calcRiftsData(controllers::RVController& rift, std::map<int32_t, int32_t>& local) {
	AION_UNPORTED();
}

int32_t RiftInformer::getTwinId(int32_t worldId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::rift
