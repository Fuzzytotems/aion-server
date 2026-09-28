#pragma once

#include <atomic>
#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::network {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.network.PffConfig
 *
 * @author Neon
 */
struct PffConfig {
	static inline std::atomic<int32_t> PFF_MODE{0};

	static inline ConfigValue<std::unordered_map<int32_t, int32_t>> THRESHOLD_MILLIS_BY_PACKET_OPCODE;

	/**
	 * @return The allowed delay in milliseconds in which two packets with the given opcode may be sent from one client.
	 */
	static int32_t getAllowedMillisBetweenPackets(int32_t opCode) {
		auto thresholds = THRESHOLD_MILLIS_BY_PACKET_OPCODE.get();
		auto it = thresholds->find(opCode);
		return it == thresholds->end() ? 0 : it->second;
	}

	/**
	 * Java: getAllowedMillisBetweenPackets(AionClientPacket) - a template until AionClientPacket is ported (any type with getOpCode()).
	 *
	 * @return The allowed delay in milliseconds in which two packets of the given type may be sent from one client.
	 */
	template <typename Packet>
	static int32_t getAllowedMillisBetweenPackets(const Packet& packet) {
		return getAllowedMillisBetweenPackets(static_cast<int32_t>(packet.getOpCode()));
	}

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::network
