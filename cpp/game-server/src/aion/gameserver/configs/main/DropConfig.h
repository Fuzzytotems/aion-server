#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <unordered_set>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.DropConfig
 *
 * @author Tiger0319, Neon
 */
struct DropConfig {
	/**
	 * Announce when a player drops an item with the configured minimum item quality
	 *
	 * @see ItemQuality
	 */
	static inline std::atomic<std::optional<detail::ItemQuality>> MIN_ANNOUNCE_QUALITY{std::nullopt};

	/** Disable range checks for specified maps */
	static inline ConfigValue<std::unordered_set<int32_t>> DISABLE_RANGE_CHECK_MAPS;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
