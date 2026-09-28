#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/services/rift/fwd.h"

namespace aion::gameserver::services::rift {

/**
 * @author Source
 */
class RiftInformer {
public:
	static void sendRiftsInfo(int32_t worldId);
	static void sendRiftsInfo(model::gameobjects::player::Player& player);
	static void sendRiftInfo(std::span<const int32_t> worlds);
	static void sendRiftDespawn(int32_t worldId, int32_t objId);
private:
	/**
	 * C++: Java builds a List<AionServerPacket> of new SM_RIFT_ANNOUNCE packets and sends it to one or many players. The packets are local to the
	 * call (K2), so the list owns them through unique_ptr.
	 */
	static std::vector<std::unique_ptr<network::aion::AionServerPacket>> getPackets(int32_t worldId);
	static std::vector<std::unique_ptr<network::aion::AionServerPacket>> getPackets(int32_t worldId, int32_t objId);
	static void syncRiftsState(model::gameobjects::player::Player& player,
		const std::vector<std::unique_ptr<network::aion::AionServerPacket>>& packets);
	static void syncRiftsState(int32_t worldId, const std::vector<std::unique_ptr<network::aion::AionServerPacket>>& packets);
	static void syncRiftsState(int32_t worldId, const std::vector<std::unique_ptr<network::aion::AionServerPacket>>& packets, bool isDespawnInfo);
	/** C++: Java TreeMap (the packet writes the entries in key order) */
	static std::map<int32_t, int32_t> getAnnounceData(int32_t worldId);
	/** Java updates `local` in place and returns it */
	static std::map<int32_t, int32_t> calcRiftsData(controllers::RVController& rift, std::map<int32_t, int32_t>& local);
	static int32_t getTwinId(int32_t worldId);
};

} // namespace aion::gameserver::services::rift
